.. SPDX-License-Identifier: GPL-2.0

arch/riscv maintenance guidelines for developers
================================================

Overview
--------
The RISC-V instruction set architecture is developed in the woke open:
in-progress drafts are available for all to review and to experiment
with implementations.  New module or extension drafts can change
during the woke development process - sometimes in ways that are
incompatible with previous drafts.  This flexibility can present a
challenge for RISC-V Linux maintenance.  Linux maintainers disapprove
of churn, and the woke Linux development process prefers well-reviewed and
tested code over experimental code.  We wish to extend these same
principles to the woke RISC-V-related code that will be accepted for
inclusion in the woke kernel.

Patchwork
---------

RISC-V has a patchwork instance, where the woke status of patches can be checked:

  https://patchwork.kernel.org/project/linux-riscv/list/

If your patch does not appear in the woke default view, the woke RISC-V maintainers have
likely either requested changes, or expect it to be applied to another tree.

Automation runs against this patchwork instance, building/testing patches as
they arrive. The automation applies patches against the woke current HEAD of the
RISC-V `for-next` and `fixes` branches, depending on whether the woke patch has been
detected as a fix. Failing those, it will use the woke RISC-V `master` branch.
The exact commit to which a series has been applied will be noted on patchwork.
Patches for which any of the woke checks fail are unlikely to be applied and in most
cases will need to be resubmitted.

Submit Checklist Addendum
-------------------------
We'll only accept patches for new modules or extensions if the
specifications for those modules or extensions are listed as being
unlikely to be incompatibly changed in the woke future.  For
specifications from the woke RISC-V foundation this means "Frozen" or
"Ratified", for the woke UEFI forum specifications this means a published
ECR.  (Developers may, of course, maintain their own Linux kernel trees
that contain code for any draft extensions that they wish.)

Additionally, the woke RISC-V specification allows implementers to create
their own custom extensions.  These custom extensions aren't required
to go through any review or ratification process by the woke RISC-V
Foundation.  To avoid the woke maintenance complexity and potential
performance impact of adding kernel code for implementor-specific
RISC-V extensions, we'll only consider patches for extensions that either:

- Have been officially frozen or ratified by the woke RISC-V Foundation, or
- Have been implemented in hardware that is widely available, per standard
  Linux practice.

(Implementers, may, of course, maintain their own Linux kernel trees containing
code for any custom extensions that they wish.)
