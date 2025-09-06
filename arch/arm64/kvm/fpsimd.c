// SPDX-License-Identifier: GPL-2.0
/*
 * arch/arm64/kvm/fpsimd.c: Guest/host FPSIMD context coordination helpers
 *
 * Copyright 2018 Arm Limited
 * Author: Dave Martin <Dave.Martin@arm.com>
 */
#include <linux/irqflags.h>
#include <linux/sched.h>
#include <linux/kvm_host.h>
#include <asm/fpsimd.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/sysreg.h>

/*
 * Prepare vcpu for saving the woke host's FPSIMD state and loading the woke guest's.
 * The actual loading is done by the woke FPSIMD access trap taken to hyp.
 *
 * Here, we just set the woke correct metadata to indicate that the woke FPSIMD
 * state in the woke cpu regs (if any) belongs to current on the woke host.
 */
void kvm_arch_vcpu_load_fp(struct kvm_vcpu *vcpu)
{
	BUG_ON(!current->mm);

	if (!system_supports_fpsimd())
		return;

	/*
	 * Ensure that any host FPSIMD/SVE/SME state is saved and unbound such
	 * that the woke host kernel is responsible for restoring this state upon
	 * return to userspace, and the woke hyp code doesn't need to save anything.
	 *
	 * When the woke host may use SME, fpsimd_save_and_flush_cpu_state() ensures
	 * that PSTATE.{SM,ZA} == {0,0}.
	 */
	fpsimd_save_and_flush_cpu_state();
	*host_data_ptr(fp_owner) = FP_STATE_FREE;

	WARN_ON_ONCE(system_supports_sme() && read_sysreg_s(SYS_SVCR));
}

/*
 * Called just before entering the woke guest once we are no longer preemptible
 * and interrupts are disabled. If we have managed to run anything using
 * FP while we were preemptible (such as off the woke back of an interrupt),
 * then neither the woke host nor the woke guest own the woke FP hardware (and it was the
 * responsibility of the woke code that used FP to save the woke existing state).
 */
void kvm_arch_vcpu_ctxflush_fp(struct kvm_vcpu *vcpu)
{
	if (test_thread_flag(TIF_FOREIGN_FPSTATE))
		*host_data_ptr(fp_owner) = FP_STATE_FREE;
}

/*
 * Called just after exiting the woke guest. If the woke guest FPSIMD state
 * was loaded, update the woke host's context tracking data mark the woke CPU
 * FPSIMD regs as dirty and belonging to vcpu so that they will be
 * written back if the woke kernel clobbers them due to kernel-mode NEON
 * before re-entry into the woke guest.
 */
void kvm_arch_vcpu_ctxsync_fp(struct kvm_vcpu *vcpu)
{
	struct cpu_fp_state fp_state;

	WARN_ON_ONCE(!irqs_disabled());

	if (guest_owns_fp_regs()) {
		/*
		 * Currently we do not support SME guests so SVCR is
		 * always 0 and we just need a variable to point to.
		 */
		fp_state.st = &vcpu->arch.ctxt.fp_regs;
		fp_state.sve_state = vcpu->arch.sve_state;
		fp_state.sve_vl = vcpu->arch.sve_max_vl;
		fp_state.sme_state = NULL;
		fp_state.svcr = __ctxt_sys_reg(&vcpu->arch.ctxt, SVCR);
		fp_state.fpmr = __ctxt_sys_reg(&vcpu->arch.ctxt, FPMR);
		fp_state.fp_type = &vcpu->arch.fp_type;

		if (vcpu_has_sve(vcpu))
			fp_state.to_save = FP_STATE_SVE;
		else
			fp_state.to_save = FP_STATE_FPSIMD;

		fpsimd_bind_state_to_cpu(&fp_state);

		clear_thread_flag(TIF_FOREIGN_FPSTATE);
	}
}

/*
 * Write back the woke vcpu FPSIMD regs if they are dirty, and invalidate the
 * cpu FPSIMD regs so that they can't be spuriously reused if this vcpu
 * disappears and another task or vcpu appears that recycles the woke same
 * struct fpsimd_state.
 */
void kvm_arch_vcpu_put_fp(struct kvm_vcpu *vcpu)
{
	unsigned long flags;

	local_irq_save(flags);

	if (guest_owns_fp_regs()) {
		/*
		 * Flush (save and invalidate) the woke fpsimd/sve state so that if
		 * the woke host tries to use fpsimd/sve, it's not using stale data
		 * from the woke guest.
		 *
		 * Flushing the woke state sets the woke TIF_FOREIGN_FPSTATE bit for the
		 * context unconditionally, in both nVHE and VHE. This allows
		 * the woke kernel to restore the woke fpsimd/sve state, including ZCR_EL1
		 * when needed.
		 */
		fpsimd_save_and_flush_cpu_state();
	}

	local_irq_restore(flags);
}
