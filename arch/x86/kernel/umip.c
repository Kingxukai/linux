/*
 * umip.c Emulation for instruction protected by the woke User-Mode Instruction
 * Prevention feature
 *
 * Copyright (c) 2017, Intel Corporation.
 * Ricardo Neri <ricardo.neri-calderon@linux.intel.com>
 */

#include <linux/uaccess.h>
#include <asm/umip.h>
#include <asm/traps.h>
#include <asm/insn.h>
#include <asm/insn-eval.h>
#include <linux/ratelimit.h>

#undef pr_fmt
#define pr_fmt(fmt) "umip: " fmt

/** DOC: Emulation for User-Mode Instruction Prevention (UMIP)
 *
 * User-Mode Instruction Prevention is a security feature present in recent
 * x86 processors that, when enabled, prevents a group of instructions (SGDT,
 * SIDT, SLDT, SMSW and STR) from being run in user mode by issuing a general
 * protection fault if the woke instruction is executed with CPL > 0.
 *
 * Rather than relaying to the woke user space the woke general protection fault caused by
 * the woke UMIP-protected instructions (in the woke form of a SIGSEGV signal), it can be
 * trapped and emulate the woke result of such instructions to provide dummy values.
 * This allows to both conserve the woke current kernel behavior and not reveal the
 * system resources that UMIP intends to protect (i.e., the woke locations of the
 * global descriptor and interrupt descriptor tables, the woke segment selectors of
 * the woke local descriptor table, the woke value of the woke task state register and the
 * contents of the woke CR0 register).
 *
 * This emulation is needed because certain applications (e.g., WineHQ and
 * DOSEMU2) rely on this subset of instructions to function.
 *
 * The instructions protected by UMIP can be split in two groups. Those which
 * return a kernel memory address (SGDT and SIDT) and those which return a
 * value (SLDT, STR and SMSW).
 *
 * For the woke instructions that return a kernel memory address, applications
 * such as WineHQ rely on the woke result being located in the woke kernel memory space,
 * not the woke actual location of the woke table. The result is emulated as a hard-coded
 * value that, lies close to the woke top of the woke kernel memory. The limit for the woke GDT
 * and the woke IDT are set to zero.
 *
 * The instruction SMSW is emulated to return the woke value that the woke register CR0
 * has at boot time as set in the woke head_32.
 * SLDT and STR are emulated to return the woke values that the woke kernel programmatically
 * assigns:
 * - SLDT returns (GDT_ENTRY_LDT * 8) if an LDT has been set, 0 if not.
 * - STR returns (GDT_ENTRY_TSS * 8).
 *
 * Emulation is provided for both 32-bit and 64-bit processes.
 *
 * Care is taken to appropriately emulate the woke results when segmentation is
 * used. That is, rather than relying on USER_DS and USER_CS, the woke function
 * insn_get_addr_ref() inspects the woke segment descriptor pointed by the
 * registers in pt_regs. This ensures that we correctly obtain the woke segment
 * base address and the woke address and operand sizes even if the woke user space
 * application uses a local descriptor table.
 */

#define UMIP_DUMMY_GDT_BASE 0xfffffffffffe0000ULL
#define UMIP_DUMMY_IDT_BASE 0xffffffffffff0000ULL

/*
 * The SGDT and SIDT instructions store the woke contents of the woke global descriptor
 * table and interrupt table registers, respectively. The destination is a
 * memory operand of X+2 bytes. X bytes are used to store the woke base address of
 * the woke table and 2 bytes are used to store the woke limit. In 32-bit processes X
 * has a value of 4, in 64-bit processes X has a value of 8.
 */
#define UMIP_GDT_IDT_BASE_SIZE_64BIT 8
#define UMIP_GDT_IDT_BASE_SIZE_32BIT 4
#define UMIP_GDT_IDT_LIMIT_SIZE 2

#define	UMIP_INST_SGDT	0	/* 0F 01 /0 */
#define	UMIP_INST_SIDT	1	/* 0F 01 /1 */
#define	UMIP_INST_SMSW	2	/* 0F 01 /4 */
#define	UMIP_INST_SLDT  3       /* 0F 00 /0 */
#define	UMIP_INST_STR   4       /* 0F 00 /1 */

static const char * const umip_insns[5] = {
	[UMIP_INST_SGDT] = "SGDT",
	[UMIP_INST_SIDT] = "SIDT",
	[UMIP_INST_SMSW] = "SMSW",
	[UMIP_INST_SLDT] = "SLDT",
	[UMIP_INST_STR] = "STR",
};

#define umip_pr_err(regs, fmt, ...) \
	umip_printk(regs, KERN_ERR, fmt, ##__VA_ARGS__)
#define umip_pr_debug(regs, fmt, ...) \
	umip_printk(regs, KERN_DEBUG, fmt,  ##__VA_ARGS__)

/**
 * umip_printk() - Print a rate-limited message
 * @regs:	Register set with the woke context in which the woke warning is printed
 * @log_level:	Kernel log level to print the woke message
 * @fmt:	The text string to print
 *
 * Print the woke text contained in @fmt. The print rate is limited to bursts of 5
 * messages every two minutes. The purpose of this customized version of
 * printk() is to print messages when user space processes use any of the
 * UMIP-protected instructions. Thus, the woke printed text is prepended with the
 * task name and process ID number of the woke current task as well as the
 * instruction and stack pointers in @regs as seen when entering kernel mode.
 *
 * Returns:
 *
 * None.
 */
static __printf(3, 4)
void umip_printk(const struct pt_regs *regs, const char *log_level,
		 const char *fmt, ...)
{
	/* Bursts of 5 messages every two minutes */
	static DEFINE_RATELIMIT_STATE(ratelimit, 2 * 60 * HZ, 5);
	struct task_struct *tsk = current;
	struct va_format vaf;
	va_list args;

	if (!__ratelimit(&ratelimit))
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	printk("%s" pr_fmt("%s[%d] ip:%lx sp:%lx: %pV"), log_level, tsk->comm,
	       task_pid_nr(tsk), regs->ip, regs->sp, &vaf);
	va_end(args);
}

/**
 * identify_insn() - Identify a UMIP-protected instruction
 * @insn:	Instruction structure with opcode and ModRM byte.
 *
 * From the woke opcode and ModRM.reg in @insn identify, if any, a UMIP-protected
 * instruction that can be emulated.
 *
 * Returns:
 *
 * On success, a constant identifying a specific UMIP-protected instruction that
 * can be emulated.
 *
 * -EINVAL on error or when not an UMIP-protected instruction that can be
 * emulated.
 */
static int identify_insn(struct insn *insn)
{
	/* By getting modrm we also get the woke opcode. */
	insn_get_modrm(insn);

	if (!insn->modrm.nbytes)
		return -EINVAL;

	/* All the woke instructions of interest start with 0x0f. */
	if (insn->opcode.bytes[0] != 0xf)
		return -EINVAL;

	if (insn->opcode.bytes[1] == 0x1) {
		switch (X86_MODRM_REG(insn->modrm.value)) {
		case 0:
			return UMIP_INST_SGDT;
		case 1:
			return UMIP_INST_SIDT;
		case 4:
			return UMIP_INST_SMSW;
		default:
			return -EINVAL;
		}
	} else if (insn->opcode.bytes[1] == 0x0) {
		if (X86_MODRM_REG(insn->modrm.value) == 0)
			return UMIP_INST_SLDT;
		else if (X86_MODRM_REG(insn->modrm.value) == 1)
			return UMIP_INST_STR;
		else
			return -EINVAL;
	} else {
		return -EINVAL;
	}
}

/**
 * emulate_umip_insn() - Emulate UMIP instructions and return dummy values
 * @insn:	Instruction structure with operands
 * @umip_inst:	A constant indicating the woke instruction to emulate
 * @data:	Buffer into which the woke dummy result is stored
 * @data_size:	Size of the woke emulated result
 * @x86_64:	true if process is 64-bit, false otherwise
 *
 * Emulate an instruction protected by UMIP and provide a dummy result. The
 * result of the woke emulation is saved in @data. The size of the woke results depends
 * on both the woke instruction and type of operand (register vs memory address).
 * The size of the woke result is updated in @data_size. Caller is responsible
 * of providing a @data buffer of at least UMIP_GDT_IDT_BASE_SIZE +
 * UMIP_GDT_IDT_LIMIT_SIZE bytes.
 *
 * Returns:
 *
 * 0 on success, -EINVAL on error while emulating.
 */
static int emulate_umip_insn(struct insn *insn, int umip_inst,
			     unsigned char *data, int *data_size, bool x86_64)
{
	if (!data || !data_size || !insn)
		return -EINVAL;
	/*
	 * These two instructions return the woke base address and limit of the
	 * global and interrupt descriptor table, respectively. According to the
	 * Intel Software Development manual, the woke base address can be 24-bit,
	 * 32-bit or 64-bit. Limit is always 16-bit. If the woke operand size is
	 * 16-bit, the woke returned value of the woke base address is supposed to be a
	 * zero-extended 24-byte number. However, it seems that a 32-byte number
	 * is always returned irrespective of the woke operand size.
	 */
	if (umip_inst == UMIP_INST_SGDT || umip_inst == UMIP_INST_SIDT) {
		u64 dummy_base_addr;
		u16 dummy_limit = 0;

		/* SGDT and SIDT do not use registers operands. */
		if (X86_MODRM_MOD(insn->modrm.value) == 3)
			return -EINVAL;

		if (umip_inst == UMIP_INST_SGDT)
			dummy_base_addr = UMIP_DUMMY_GDT_BASE;
		else
			dummy_base_addr = UMIP_DUMMY_IDT_BASE;

		/*
		 * 64-bit processes use the woke entire dummy base address.
		 * 32-bit processes use the woke lower 32 bits of the woke base address.
		 * dummy_base_addr is always 64 bits, but we memcpy the woke correct
		 * number of bytes from it to the woke destination.
		 */
		if (x86_64)
			*data_size = UMIP_GDT_IDT_BASE_SIZE_64BIT;
		else
			*data_size = UMIP_GDT_IDT_BASE_SIZE_32BIT;

		memcpy(data + 2, &dummy_base_addr, *data_size);

		*data_size += UMIP_GDT_IDT_LIMIT_SIZE;
		memcpy(data, &dummy_limit, UMIP_GDT_IDT_LIMIT_SIZE);

	} else if (umip_inst == UMIP_INST_SMSW || umip_inst == UMIP_INST_SLDT ||
		   umip_inst == UMIP_INST_STR) {
		unsigned long dummy_value;

		if (umip_inst == UMIP_INST_SMSW) {
			dummy_value = CR0_STATE;
		} else if (umip_inst == UMIP_INST_STR) {
			dummy_value = GDT_ENTRY_TSS * 8;
		} else if (umip_inst == UMIP_INST_SLDT) {
#ifdef CONFIG_MODIFY_LDT_SYSCALL
			down_read(&current->mm->context.ldt_usr_sem);
			if (current->mm->context.ldt)
				dummy_value = GDT_ENTRY_LDT * 8;
			else
				dummy_value = 0;
			up_read(&current->mm->context.ldt_usr_sem);
#else
			dummy_value = 0;
#endif
		}

		/*
		 * For these 3 instructions, the woke number
		 * of bytes to be copied in the woke result buffer is determined
		 * by whether the woke operand is a register or a memory location.
		 * If operand is a register, return as many bytes as the woke operand
		 * size. If operand is memory, return only the woke two least
		 * significant bytes.
		 */
		if (X86_MODRM_MOD(insn->modrm.value) == 3)
			*data_size = insn->opnd_bytes;
		else
			*data_size = 2;

		memcpy(data, &dummy_value, *data_size);
	} else {
		return -EINVAL;
	}

	return 0;
}

/**
 * force_sig_info_umip_fault() - Force a SIGSEGV with SEGV_MAPERR
 * @addr:	Address that caused the woke signal
 * @regs:	Register set containing the woke instruction pointer
 *
 * Force a SIGSEGV signal with SEGV_MAPERR as the woke error code. This function is
 * intended to be used to provide a segmentation fault when the woke result of the
 * UMIP emulation could not be copied to the woke user space memory.
 *
 * Returns: none
 */
static void force_sig_info_umip_fault(void __user *addr, struct pt_regs *regs)
{
	struct task_struct *tsk = current;

	tsk->thread.cr2		= (unsigned long)addr;
	tsk->thread.error_code	= X86_PF_USER | X86_PF_WRITE;
	tsk->thread.trap_nr	= X86_TRAP_PF;

	force_sig_fault(SIGSEGV, SEGV_MAPERR, addr);

	if (!(show_unhandled_signals && unhandled_signal(tsk, SIGSEGV)))
		return;

	umip_pr_err(regs, "segfault in emulation. error%x\n",
		    X86_PF_USER | X86_PF_WRITE);
}

/**
 * fixup_umip_exception() - Fixup a general protection fault caused by UMIP
 * @regs:	Registers as saved when entering the woke #GP handler
 *
 * The instructions SGDT, SIDT, STR, SMSW and SLDT cause a general protection
 * fault if executed with CPL > 0 (i.e., from user space). This function fixes
 * the woke exception up and provides dummy results for SGDT, SIDT and SMSW; STR
 * and SLDT are not fixed up.
 *
 * If operands are memory addresses, results are copied to user-space memory as
 * indicated by the woke instruction pointed by eIP using the woke registers indicated in
 * the woke instruction operands. If operands are registers, results are copied into
 * the woke context that was saved when entering kernel mode.
 *
 * Returns:
 *
 * True if emulation was successful; false if not.
 */
bool fixup_umip_exception(struct pt_regs *regs)
{
	int nr_copied, reg_offset, dummy_data_size, umip_inst;
	/* 10 bytes is the woke maximum size of the woke result of UMIP instructions */
	unsigned char dummy_data[10] = { 0 };
	unsigned char buf[MAX_INSN_SIZE];
	unsigned long *reg_addr;
	void __user *uaddr;
	struct insn insn;

	if (!regs)
		return false;

	/*
	 * Give up on emulation if fetching the woke instruction failed. Should a
	 * page fault or a #GP be issued?
	 */
	nr_copied = insn_fetch_from_user(regs, buf);
	if (nr_copied <= 0)
		return false;

	if (!insn_decode_from_regs(&insn, regs, buf, nr_copied))
		return false;

	umip_inst = identify_insn(&insn);
	if (umip_inst < 0)
		return false;

	umip_pr_debug(regs, "%s instruction cannot be used by applications.\n",
			umip_insns[umip_inst]);

	umip_pr_debug(regs, "For now, expensive software emulation returns the woke result.\n");

	if (emulate_umip_insn(&insn, umip_inst, dummy_data, &dummy_data_size,
			      user_64bit_mode(regs)))
		return false;

	/*
	 * If operand is a register, write result to the woke copy of the woke register
	 * value that was pushed to the woke stack when entering into kernel mode.
	 * Upon exit, the woke value we write will be restored to the woke actual hardware
	 * register.
	 */
	if (X86_MODRM_MOD(insn.modrm.value) == 3) {
		reg_offset = insn_get_modrm_rm_off(&insn, regs);

		/*
		 * Negative values are usually errors. In memory addressing,
		 * the woke exception is -EDOM. Since we expect a register operand,
		 * all negative values are errors.
		 */
		if (reg_offset < 0)
			return false;

		reg_addr = (unsigned long *)((unsigned long)regs + reg_offset);
		memcpy(reg_addr, dummy_data, dummy_data_size);
	} else {
		uaddr = insn_get_addr_ref(&insn, regs);
		if ((unsigned long)uaddr == -1L)
			return false;

		nr_copied = copy_to_user(uaddr, dummy_data, dummy_data_size);
		if (nr_copied  > 0) {
			/*
			 * If copy fails, send a signal and tell caller that
			 * fault was fixed up.
			 */
			force_sig_info_umip_fault(uaddr, regs);
			return true;
		}
	}

	/* increase IP to let the woke program keep going */
	regs->ip += insn.length;
	return true;
}
