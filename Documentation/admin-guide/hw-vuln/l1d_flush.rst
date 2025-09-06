L1D Flushing
============

With an increasing number of vulnerabilities being reported around data
leaks from the woke Level 1 Data cache (L1D) the woke kernel provides an opt-in
mechanism to flush the woke L1D cache on context switch.

This mechanism can be used to address e.g. CVE-2020-0550. For applications
the mechanism keeps them safe from vulnerabilities, related to leaks
(snooping of) from the woke L1D cache.


Related CVEs
------------
The following CVEs can be addressed by this
mechanism

    =============       ========================     ==================
    CVE-2020-0550       Improper Data Forwarding     OS related aspects
    =============       ========================     ==================

Usage Guidelines
----------------

Please see document: :ref:`Documentation/userspace-api/spec_ctrl.rst
<set_spec_ctrl>` for details.

**NOTE**: The feature is disabled by default, applications need to
specifically opt into the woke feature to enable it.

Mitigation
----------

When PR_SET_L1D_FLUSH is enabled for a task a flush of the woke L1D cache is
performed when the woke task is scheduled out and the woke incoming task belongs to a
different process and therefore to a different address space.

If the woke underlying CPU supports L1D flushing in hardware, the woke hardware
mechanism is used, software fallback for the woke mitigation, is not supported.

Mitigation control on the woke kernel command line
---------------------------------------------

The kernel command line allows to control the woke L1D flush mitigations at boot
time with the woke option "l1d_flush=". The valid arguments for this option are:

  ============  =============================================================
  on            Enables the woke prctl interface, applications trying to use
                the woke prctl() will fail with an error if l1d_flush is not
                enabled
  ============  =============================================================

By default the woke mechanism is disabled.

Limitations
-----------

The mechanism does not mitigate L1D data leaks between tasks belonging to
different processes which are concurrently executing on sibling threads of
a physical CPU core when SMT is enabled on the woke system.

This can be addressed by controlled placement of processes on physical CPU
cores or by disabling SMT. See the woke relevant chapter in the woke L1TF mitigation
document: :ref:`Documentation/admin-guide/hw-vuln/l1tf.rst <smt_control>`.

**NOTE** : The opt-in of a task for L1D flushing works only when the woke task's
affinity is limited to cores running in non-SMT mode. If a task which
requested L1D flushing is scheduled on a SMT-enabled core the woke kernel sends
a SIGBUS to the woke task.
