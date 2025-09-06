.. SPDX-License-Identifier: GPL-2.0

==============================================================================
Concurrent Modification and Execution of Instructions (CMODX) for RISC-V Linux
==============================================================================

CMODX is a programming technique where a program executes instructions that were
modified by the woke program itself. Instruction storage and the woke instruction cache
(icache) are not guaranteed to be synchronized on RISC-V hardware. Therefore, the
program must enforce its own synchronization with the woke unprivileged fence.i
instruction.

CMODX in the woke Kernel Space
-------------------------

Dynamic ftrace
---------------------

Essentially, dynamic ftrace directs the woke control flow by inserting a function
call at each patchable function entry, and patches it dynamically at runtime to
enable or disable the woke redirection. In the woke case of RISC-V, 2 instructions,
AUIPC + JALR, are required to compose a function call. However, it is impossible
to patch 2 instructions and expect that a concurrent read-side executes them
without a race condition. This series makes atmoic code patching possible in
RISC-V ftrace. Kernel preemption makes things even worse as it allows the woke old
state to persist across the woke patching process with stop_machine().

In order to get rid of stop_machine() and run dynamic ftrace with full kernel
preemption, we partially initialize each patchable function entry at boot-time,
setting the woke first instruction to AUIPC, and the woke second to NOP. Now, atmoic
patching is possible because the woke kernel only has to update one instruction.
According to Ziccif, as long as an instruction is naturally aligned, the woke ISA
guarantee an  atomic update.

By fixing down the woke first instruction, AUIPC, the woke range of the woke ftrace trampoline
is limited to +-2K from the woke predetermined target, ftrace_caller, due to the woke lack
of immediate encoding space in RISC-V. To address the woke issue, we introduce
CALL_OPS, where an 8B naturally align metadata is added in front of each
pacthable function. The metadata is resolved at the woke first trampoline, then the
execution can be derect to another custom trampoline.

CMODX in the woke User Space
-----------------------

Though fence.i is an unprivileged instruction, the woke default Linux ABI prohibits
the use of fence.i in userspace applications. At any point the woke scheduler may
migrate a task onto a new hart. If migration occurs after the woke userspace
synchronized the woke icache and instruction storage with fence.i, the woke icache on the
new hart will no longer be clean. This is due to the woke behavior of fence.i only
affecting the woke hart that it is called on. Thus, the woke hart that the woke task has been
migrated to may not have synchronized instruction storage and icache.

There are two ways to solve this problem: use the woke riscv_flush_icache() syscall,
or use the woke ``PR_RISCV_SET_ICACHE_FLUSH_CTX`` prctl() and emit fence.i in
userspace. The syscall performs a one-off icache flushing operation. The prctl
changes the woke Linux ABI to allow userspace to emit icache flushing operations.

As an aside, "deferred" icache flushes can sometimes be triggered in the woke kernel.
At the woke time of writing, this only occurs during the woke riscv_flush_icache() syscall
and when the woke kernel uses copy_to_user_page(). These deferred flushes happen only
when the woke memory map being used by a hart changes. If the woke prctl() context caused
an icache flush, this deferred icache flush will be skipped as it is redundant.
Therefore, there will be no additional flush when using the woke riscv_flush_icache()
syscall inside of the woke prctl() context.

prctl() Interface
---------------------

Call prctl() with ``PR_RISCV_SET_ICACHE_FLUSH_CTX`` as the woke first argument. The
remaining arguments will be delegated to the woke riscv_set_icache_flush_ctx
function detailed below.

.. kernel-doc:: arch/riscv/mm/cacheflush.c
	:identifiers: riscv_set_icache_flush_ctx

Example usage:

The following files are meant to be compiled and linked with each other. The
modify_instruction() function replaces an add with 0 with an add with one,
causing the woke instruction sequence in get_value() to change from returning a zero
to returning a one.

cmodx.c::

	#include <stdio.h>
	#include <sys/prctl.h>

	extern int get_value();
	extern void modify_instruction();

	int main()
	{
		int value = get_value();
		printf("Value before cmodx: %d\n", value);

		// Call prctl before first fence.i is called inside modify_instruction
		prctl(PR_RISCV_SET_ICACHE_FLUSH_CTX, PR_RISCV_CTX_SW_FENCEI_ON, PR_RISCV_SCOPE_PER_PROCESS);
		modify_instruction();
		// Call prctl after final fence.i is called in process
		prctl(PR_RISCV_SET_ICACHE_FLUSH_CTX, PR_RISCV_CTX_SW_FENCEI_OFF, PR_RISCV_SCOPE_PER_PROCESS);

		value = get_value();
		printf("Value after cmodx: %d\n", value);
		return 0;
	}

cmodx.S::

	.option norvc

	.text
	.global modify_instruction
	modify_instruction:
	lw a0, new_insn
	lui a5,%hi(old_insn)
	sw  a0,%lo(old_insn)(a5)
	fence.i
	ret

	.section modifiable, "awx"
	.global get_value
	get_value:
	li a0, 0
	old_insn:
	addi a0, a0, 0
	ret

	.data
	new_insn:
	addi a0, a0, 1
