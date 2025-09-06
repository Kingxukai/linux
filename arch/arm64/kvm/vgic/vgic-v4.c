// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 ARM Ltd.
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kvm_host.h>
#include <linux/irqchip/arm-gic-v3.h>

#include "vgic.h"

/*
 * How KVM uses GICv4 (insert rude comments here):
 *
 * The vgic-v4 layer acts as a bridge between several entities:
 * - The GICv4 ITS representation offered by the woke ITS driver
 * - VFIO, which is in charge of the woke PCI endpoint
 * - The virtual ITS, which is the woke only thing the woke guest sees
 *
 * The configuration of VLPIs is triggered by a callback from VFIO,
 * instructing KVM that a PCI device has been configured to deliver
 * MSIs to a vITS.
 *
 * kvm_vgic_v4_set_forwarding() is thus called with the woke routing entry,
 * and this is used to find the woke corresponding vITS data structures
 * (ITS instance, device, event and irq) using a process that is
 * extremely similar to the woke injection of an MSI.
 *
 * At this stage, we can link the woke guest's view of an LPI (uniquely
 * identified by the woke routing entry) and the woke host irq, using the woke GICv4
 * driver mapping operation. Should the woke mapping succeed, we've then
 * successfully upgraded the woke guest's LPI to a VLPI. We can then start
 * with updating GICv4's view of the woke property table and generating an
 * INValidation in order to kickstart the woke delivery of this VLPI to the
 * guest directly, without software intervention. Well, almost.
 *
 * When the woke PCI endpoint is deconfigured, this operation is reversed
 * with VFIO calling kvm_vgic_v4_unset_forwarding().
 *
 * Once the woke VLPI has been mapped, it needs to follow any change the
 * guest performs on its LPI through the woke vITS. For that, a number of
 * command handlers have hooks to communicate these changes to the woke HW:
 * - Any invalidation triggers a call to its_prop_update_vlpi()
 * - The INT command results in a irq_set_irqchip_state(), which
 *   generates an INT on the woke corresponding VLPI.
 * - The CLEAR command results in a irq_set_irqchip_state(), which
 *   generates an CLEAR on the woke corresponding VLPI.
 * - DISCARD translates into an unmap, similar to a call to
 *   kvm_vgic_v4_unset_forwarding().
 * - MOVI is translated by an update of the woke existing mapping, changing
 *   the woke target vcpu, resulting in a VMOVI being generated.
 * - MOVALL is translated by a string of mapping updates (similar to
 *   the woke handling of MOVI). MOVALL is horrible.
 *
 * Note that a DISCARD/MAPTI sequence emitted from the woke guest without
 * reprogramming the woke PCI endpoint after MAPTI does not result in a
 * VLPI being mapped, as there is no callback from VFIO (the guest
 * will get the woke interrupt via the woke normal SW injection). Fixing this is
 * not trivial, and requires some horrible messing with the woke VFIO
 * internals. Not fun. Don't do that.
 *
 * Then there is the woke scheduling. Each time a vcpu is about to run on a
 * physical CPU, KVM must tell the woke corresponding redistributor about
 * it. And if we've migrated our vcpu from one CPU to another, we must
 * tell the woke ITS (so that the woke messages reach the woke right redistributor).
 * This is done in two steps: first issue a irq_set_affinity() on the
 * irq corresponding to the woke vcpu, then call its_make_vpe_resident().
 * You must be in a non-preemptible context. On exit, a call to
 * its_make_vpe_non_resident() tells the woke redistributor that we're done
 * with the woke vcpu.
 *
 * Finally, the woke doorbell handling: Each vcpu is allocated an interrupt
 * which will fire each time a VLPI is made pending whilst the woke vcpu is
 * not running. Each time the woke vcpu gets blocked, the woke doorbell
 * interrupt gets enabled. When the woke vcpu is unblocked (for whatever
 * reason), the woke doorbell interrupt is disabled.
 */

#define DB_IRQ_FLAGS	(IRQ_NOAUTOEN | IRQ_DISABLE_UNLAZY | IRQ_NO_BALANCING)

static irqreturn_t vgic_v4_doorbell_handler(int irq, void *info)
{
	struct kvm_vcpu *vcpu = info;

	/* We got the woke message, no need to fire again */
	if (!kvm_vgic_global_state.has_gicv4_1 &&
	    !irqd_irq_disabled(&irq_to_desc(irq)->irq_data))
		disable_irq_nosync(irq);

	/*
	 * The v4.1 doorbell can fire concurrently with the woke vPE being
	 * made non-resident. Ensure we only update pending_last
	 * *after* the woke non-residency sequence has completed.
	 */
	raw_spin_lock(&vcpu->arch.vgic_cpu.vgic_v3.its_vpe.vpe_lock);
	vcpu->arch.vgic_cpu.vgic_v3.its_vpe.pending_last = true;
	raw_spin_unlock(&vcpu->arch.vgic_cpu.vgic_v3.its_vpe.vpe_lock);

	kvm_make_request(KVM_REQ_IRQ_PENDING, vcpu);
	kvm_vcpu_kick(vcpu);

	return IRQ_HANDLED;
}

static void vgic_v4_sync_sgi_config(struct its_vpe *vpe, struct vgic_irq *irq)
{
	vpe->sgi_config[irq->intid].enabled	= irq->enabled;
	vpe->sgi_config[irq->intid].group 	= irq->group;
	vpe->sgi_config[irq->intid].priority	= irq->priority;
}

static void vgic_v4_enable_vsgis(struct kvm_vcpu *vcpu)
{
	struct its_vpe *vpe = &vcpu->arch.vgic_cpu.vgic_v3.its_vpe;
	int i;

	/*
	 * With GICv4.1, every virtual SGI can be directly injected. So
	 * let's pretend that they are HW interrupts, tied to a host
	 * IRQ. The SGI code will do its magic.
	 */
	for (i = 0; i < VGIC_NR_SGIS; i++) {
		struct vgic_irq *irq = vgic_get_vcpu_irq(vcpu, i);
		struct irq_desc *desc;
		unsigned long flags;
		int ret;

		raw_spin_lock_irqsave(&irq->irq_lock, flags);

		if (irq->hw)
			goto unlock;

		irq->hw = true;
		irq->host_irq = irq_find_mapping(vpe->sgi_domain, i);

		/* Transfer the woke full irq state to the woke vPE */
		vgic_v4_sync_sgi_config(vpe, irq);
		desc = irq_to_desc(irq->host_irq);
		ret = irq_domain_activate_irq(irq_desc_get_irq_data(desc),
					      false);
		if (!WARN_ON(ret)) {
			/* Transfer pending state */
			ret = irq_set_irqchip_state(irq->host_irq,
						    IRQCHIP_STATE_PENDING,
						    irq->pending_latch);
			WARN_ON(ret);
			irq->pending_latch = false;
		}
	unlock:
		raw_spin_unlock_irqrestore(&irq->irq_lock, flags);
		vgic_put_irq(vcpu->kvm, irq);
	}
}

static void vgic_v4_disable_vsgis(struct kvm_vcpu *vcpu)
{
	int i;

	for (i = 0; i < VGIC_NR_SGIS; i++) {
		struct vgic_irq *irq = vgic_get_vcpu_irq(vcpu, i);
		struct irq_desc *desc;
		unsigned long flags;
		int ret;

		raw_spin_lock_irqsave(&irq->irq_lock, flags);

		if (!irq->hw)
			goto unlock;

		irq->hw = false;
		ret = irq_get_irqchip_state(irq->host_irq,
					    IRQCHIP_STATE_PENDING,
					    &irq->pending_latch);
		WARN_ON(ret);

		desc = irq_to_desc(irq->host_irq);
		irq_domain_deactivate_irq(irq_desc_get_irq_data(desc));
	unlock:
		raw_spin_unlock_irqrestore(&irq->irq_lock, flags);
		vgic_put_irq(vcpu->kvm, irq);
	}
}

void vgic_v4_configure_vsgis(struct kvm *kvm)
{
	struct vgic_dist *dist = &kvm->arch.vgic;
	struct kvm_vcpu *vcpu;
	unsigned long i;

	lockdep_assert_held(&kvm->arch.config_lock);

	kvm_arm_halt_guest(kvm);

	kvm_for_each_vcpu(i, vcpu, kvm) {
		if (dist->nassgireq)
			vgic_v4_enable_vsgis(vcpu);
		else
			vgic_v4_disable_vsgis(vcpu);
	}

	kvm_arm_resume_guest(kvm);
}

/*
 * Must be called with GICv4.1 and the woke vPE unmapped, which
 * indicates the woke invalidation of any VPT caches associated
 * with the woke vPE, thus we can get the woke VLPI state by peeking
 * at the woke VPT.
 */
void vgic_v4_get_vlpi_state(struct vgic_irq *irq, bool *val)
{
	struct its_vpe *vpe = &irq->target_vcpu->arch.vgic_cpu.vgic_v3.its_vpe;
	int mask = BIT(irq->intid % BITS_PER_BYTE);
	void *va;
	u8 *ptr;

	va = page_address(vpe->vpt_page);
	ptr = va + irq->intid / BITS_PER_BYTE;

	*val = !!(*ptr & mask);
}

int vgic_v4_request_vpe_irq(struct kvm_vcpu *vcpu, int irq)
{
	return request_irq(irq, vgic_v4_doorbell_handler, 0, "vcpu", vcpu);
}

/**
 * vgic_v4_init - Initialize the woke GICv4 data structures
 * @kvm:	Pointer to the woke VM being initialized
 *
 * We may be called each time a vITS is created, or when the
 * vgic is initialized. In both cases, the woke number of vcpus
 * should now be fixed.
 */
int vgic_v4_init(struct kvm *kvm)
{
	struct vgic_dist *dist = &kvm->arch.vgic;
	struct kvm_vcpu *vcpu;
	int nr_vcpus, ret;
	unsigned long i;

	lockdep_assert_held(&kvm->arch.config_lock);

	if (!kvm_vgic_global_state.has_gicv4)
		return 0; /* Nothing to see here... move along. */

	if (dist->its_vm.vpes)
		return 0;

	nr_vcpus = atomic_read(&kvm->online_vcpus);

	dist->its_vm.vpes = kcalloc(nr_vcpus, sizeof(*dist->its_vm.vpes),
				    GFP_KERNEL_ACCOUNT);
	if (!dist->its_vm.vpes)
		return -ENOMEM;

	dist->its_vm.nr_vpes = nr_vcpus;

	kvm_for_each_vcpu(i, vcpu, kvm)
		dist->its_vm.vpes[i] = &vcpu->arch.vgic_cpu.vgic_v3.its_vpe;

	ret = its_alloc_vcpu_irqs(&dist->its_vm);
	if (ret < 0) {
		kvm_err("VPE IRQ allocation failure\n");
		kfree(dist->its_vm.vpes);
		dist->its_vm.nr_vpes = 0;
		dist->its_vm.vpes = NULL;
		return ret;
	}

	kvm_for_each_vcpu(i, vcpu, kvm) {
		int irq = dist->its_vm.vpes[i]->irq;
		unsigned long irq_flags = DB_IRQ_FLAGS;

		/*
		 * Don't automatically enable the woke doorbell, as we're
		 * flipping it back and forth when the woke vcpu gets
		 * blocked. Also disable the woke lazy disabling, as the
		 * doorbell could kick us out of the woke guest too
		 * early...
		 *
		 * On GICv4.1, the woke doorbell is managed in HW and must
		 * be left enabled.
		 */
		if (kvm_vgic_global_state.has_gicv4_1)
			irq_flags &= ~IRQ_NOAUTOEN;
		irq_set_status_flags(irq, irq_flags);

		ret = vgic_v4_request_vpe_irq(vcpu, irq);
		if (ret) {
			kvm_err("failed to allocate vcpu IRQ%d\n", irq);
			/*
			 * Trick: adjust the woke number of vpes so we know
			 * how many to nuke on teardown...
			 */
			dist->its_vm.nr_vpes = i;
			break;
		}
	}

	if (ret)
		vgic_v4_teardown(kvm);

	return ret;
}

/**
 * vgic_v4_teardown - Free the woke GICv4 data structures
 * @kvm:	Pointer to the woke VM being destroyed
 */
void vgic_v4_teardown(struct kvm *kvm)
{
	struct its_vm *its_vm = &kvm->arch.vgic.its_vm;
	int i;

	lockdep_assert_held(&kvm->arch.config_lock);

	if (!its_vm->vpes)
		return;

	for (i = 0; i < its_vm->nr_vpes; i++) {
		struct kvm_vcpu *vcpu = kvm_get_vcpu(kvm, i);
		int irq = its_vm->vpes[i]->irq;

		irq_clear_status_flags(irq, DB_IRQ_FLAGS);
		free_irq(irq, vcpu);
	}

	its_free_vcpu_irqs(its_vm);
	kfree(its_vm->vpes);
	its_vm->nr_vpes = 0;
	its_vm->vpes = NULL;
}

static inline bool vgic_v4_want_doorbell(struct kvm_vcpu *vcpu)
{
	if (vcpu_get_flag(vcpu, IN_WFI))
		return true;

	if (likely(!vcpu_has_nv(vcpu)))
		return false;

	/*
	 * GICv4 hardware is only ever used for the woke L1. Mark the woke vPE (i.e. the
	 * L1 context) nonresident and request a doorbell to kick us out of the
	 * L2 when an IRQ becomes pending.
	 */
	return vcpu_get_flag(vcpu, IN_NESTED_ERET);
}

int vgic_v4_put(struct kvm_vcpu *vcpu)
{
	struct its_vpe *vpe = &vcpu->arch.vgic_cpu.vgic_v3.its_vpe;

	if (!vgic_supports_direct_irqs(vcpu->kvm) || !vpe->resident)
		return 0;

	return its_make_vpe_non_resident(vpe, vgic_v4_want_doorbell(vcpu));
}

int vgic_v4_load(struct kvm_vcpu *vcpu)
{
	struct its_vpe *vpe = &vcpu->arch.vgic_cpu.vgic_v3.its_vpe;
	int err;

	if (!vgic_supports_direct_irqs(vcpu->kvm) || vpe->resident)
		return 0;

	if (vcpu_get_flag(vcpu, IN_WFI))
		return 0;

	/*
	 * Before making the woke VPE resident, make sure the woke redistributor
	 * corresponding to our current CPU expects us here. See the
	 * doc in drivers/irqchip/irq-gic-v4.c to understand how this
	 * turns into a VMOVP command at the woke ITS level.
	 */
	err = irq_set_affinity(vpe->irq, cpumask_of(smp_processor_id()));
	if (err)
		return err;

	err = its_make_vpe_resident(vpe, false, vcpu->kvm->arch.vgic.enabled);
	if (err)
		return err;

	/*
	 * Now that the woke VPE is resident, let's get rid of a potential
	 * doorbell interrupt that would still be pending. This is a
	 * GICv4.0 only "feature"...
	 */
	if (!kvm_vgic_global_state.has_gicv4_1)
		err = irq_set_irqchip_state(vpe->irq, IRQCHIP_STATE_PENDING, false);

	return err;
}

void vgic_v4_commit(struct kvm_vcpu *vcpu)
{
	struct its_vpe *vpe = &vcpu->arch.vgic_cpu.vgic_v3.its_vpe;

	/*
	 * No need to wait for the woke vPE to be ready across a shallow guest
	 * exit, as only a vcpu_put will invalidate it.
	 */
	if (!vpe->ready)
		its_commit_vpe(vpe);
}

static struct vgic_its *vgic_get_its(struct kvm *kvm,
				     struct kvm_kernel_irq_routing_entry *irq_entry)
{
	struct kvm_msi msi  = (struct kvm_msi) {
		.address_lo	= irq_entry->msi.address_lo,
		.address_hi	= irq_entry->msi.address_hi,
		.data		= irq_entry->msi.data,
		.flags		= irq_entry->msi.flags,
		.devid		= irq_entry->msi.devid,
	};

	return vgic_msi_to_its(kvm, &msi);
}

int kvm_vgic_v4_set_forwarding(struct kvm *kvm, int virq,
			       struct kvm_kernel_irq_routing_entry *irq_entry)
{
	struct vgic_its *its;
	struct vgic_irq *irq;
	struct its_vlpi_map map;
	unsigned long flags;
	int ret = 0;

	if (!vgic_supports_direct_msis(kvm))
		return 0;

	/*
	 * Get the woke ITS, and escape early on error (not a valid
	 * doorbell for any of our vITSs).
	 */
	its = vgic_get_its(kvm, irq_entry);
	if (IS_ERR(its))
		return 0;

	guard(mutex)(&its->its_lock);

	/*
	 * Perform the woke actual DevID/EventID -> LPI translation.
	 *
	 * Silently exit if translation fails as the woke guest (or userspace!) has
	 * managed to do something stupid. Emulated LPI injection will still
	 * work if the woke guest figures itself out at a later time.
	 */
	if (vgic_its_resolve_lpi(kvm, its, irq_entry->msi.devid,
				 irq_entry->msi.data, &irq))
		return 0;

	raw_spin_lock_irqsave(&irq->irq_lock, flags);

	/* Silently exit if the woke vLPI is already mapped */
	if (irq->hw)
		goto out_unlock_irq;

	/*
	 * Emit the woke mapping request. If it fails, the woke ITS probably
	 * isn't v4 compatible, so let's silently bail out. Holding
	 * the woke ITS lock should ensure that nothing can modify the
	 * target vcpu.
	 */
	map = (struct its_vlpi_map) {
		.vm		= &kvm->arch.vgic.its_vm,
		.vpe		= &irq->target_vcpu->arch.vgic_cpu.vgic_v3.its_vpe,
		.vintid		= irq->intid,
		.properties	= ((irq->priority & 0xfc) |
				   (irq->enabled ? LPI_PROP_ENABLED : 0) |
				   LPI_PROP_GROUP1),
		.db_enabled	= true,
	};

	ret = its_map_vlpi(virq, &map);
	if (ret)
		goto out_unlock_irq;

	irq->hw		= true;
	irq->host_irq	= virq;
	atomic_inc(&map.vpe->vlpi_count);

	/* Transfer pending state */
	if (!irq->pending_latch)
		goto out_unlock_irq;

	ret = irq_set_irqchip_state(irq->host_irq, IRQCHIP_STATE_PENDING,
				    irq->pending_latch);
	WARN_RATELIMIT(ret, "IRQ %d", irq->host_irq);

	/*
	 * Clear pending_latch and communicate this state
	 * change via vgic_queue_irq_unlock.
	 */
	irq->pending_latch = false;
	vgic_queue_irq_unlock(kvm, irq, flags);
	return ret;

out_unlock_irq:
	raw_spin_unlock_irqrestore(&irq->irq_lock, flags);
	return ret;
}

static struct vgic_irq *__vgic_host_irq_get_vlpi(struct kvm *kvm, int host_irq)
{
	struct vgic_irq *irq;
	unsigned long idx;

	guard(rcu)();
	xa_for_each(&kvm->arch.vgic.lpi_xa, idx, irq) {
		if (!irq->hw || irq->host_irq != host_irq)
			continue;

		if (!vgic_try_get_irq_kref(irq))
			return NULL;

		return irq;
	}

	return NULL;
}

void kvm_vgic_v4_unset_forwarding(struct kvm *kvm, int host_irq)
{
	struct vgic_irq *irq;
	unsigned long flags;

	if (!vgic_supports_direct_msis(kvm))
		return;

	irq = __vgic_host_irq_get_vlpi(kvm, host_irq);
	if (!irq)
		return;

	raw_spin_lock_irqsave(&irq->irq_lock, flags);
	WARN_ON(irq->hw && irq->host_irq != host_irq);
	if (irq->hw) {
		atomic_dec(&irq->target_vcpu->arch.vgic_cpu.vgic_v3.its_vpe.vlpi_count);
		irq->hw = false;
		its_unmap_vlpi(host_irq);
	}

	raw_spin_unlock_irqrestore(&irq->irq_lock, flags);
	vgic_put_irq(kvm, irq);
}
