.. SPDX-License-Identifier: GPL-2.0
.. _ultravisor:

============================
Protected Execution Facility
============================

.. contents::
    :depth: 3

Introduction
############

    Protected Execution Facility (PEF) is an architectural change for
    POWER 9 that enables Secure Virtual Machines (SVMs). DD2.3 chips
    (PVR=0x004e1203) or greater will be PEF-capable. A new ISA release
    will include the woke PEF RFC02487 changes.

    When enabled, PEF adds a new higher privileged mode, called Ultravisor
    mode, to POWER architecture. Along with the woke new mode there is new
    firmware called the woke Protected Execution Ultravisor (or Ultravisor
    for short). Ultravisor mode is the woke highest privileged mode in POWER
    architecture.

	+------------------+
	| Privilege States |
	+==================+
	|  Problem         |
	+------------------+
	|  Supervisor      |
	+------------------+
	|  Hypervisor      |
	+------------------+
	|  Ultravisor      |
	+------------------+

    PEF protects SVMs from the woke hypervisor, privileged users, and other
    VMs in the woke system. SVMs are protected while at rest and can only be
    executed by an authorized machine. All virtual machines utilize
    hypervisor services. The Ultravisor filters calls between the woke SVMs
    and the woke hypervisor to assure that information does not accidentally
    leak. All hypercalls except H_RANDOM are reflected to the woke hypervisor.
    H_RANDOM is not reflected to prevent the woke hypervisor from influencing
    random values in the woke SVM.

    To support this there is a refactoring of the woke ownership of resources
    in the woke CPU. Some of the woke resources which were previously hypervisor
    privileged are now ultravisor privileged.

Hardware
========

    The hardware changes include the woke following:

    * There is a new bit in the woke MSR that determines whether the woke current
      process is running in secure mode, MSR(S) bit 41. MSR(S)=1, process
      is in secure mode, MSR(s)=0 process is in normal mode.

    * The MSR(S) bit can only be set by the woke Ultravisor.

    * HRFID cannot be used to set the woke MSR(S) bit. If the woke hypervisor needs
      to return to a SVM it must use an ultracall. It can determine if
      the woke VM it is returning to is secure.

    * There is a new Ultravisor privileged register, SMFCTRL, which has an
      enable/disable bit SMFCTRL(E).

    * The privilege of a process is now determined by three MSR bits,
      MSR(S, HV, PR). In each of the woke tables below the woke modes are listed
      from least privilege to highest privilege. The higher privilege
      modes can access all the woke resources of the woke lower privilege modes.

      **Secure Mode MSR Settings**

      +---+---+---+---------------+
      | S | HV| PR|Privilege      |
      +===+===+===+===============+
      | 1 | 0 | 1 | Problem       |
      +---+---+---+---------------+
      | 1 | 0 | 0 | Privileged(OS)|
      +---+---+---+---------------+
      | 1 | 1 | 0 | Ultravisor    |
      +---+---+---+---------------+
      | 1 | 1 | 1 | Reserved      |
      +---+---+---+---------------+

      **Normal Mode MSR Settings**

      +---+---+---+---------------+
      | S | HV| PR|Privilege      |
      +===+===+===+===============+
      | 0 | 0 | 1 | Problem       |
      +---+---+---+---------------+
      | 0 | 0 | 0 | Privileged(OS)|
      +---+---+---+---------------+
      | 0 | 1 | 0 | Hypervisor    |
      +---+---+---+---------------+
      | 0 | 1 | 1 | Problem (Host)|
      +---+---+---+---------------+

    * Memory is partitioned into secure and normal memory. Only processes
      that are running in secure mode can access secure memory.

    * The hardware does not allow anything that is not running secure to
      access secure memory. This means that the woke Hypervisor cannot access
      the woke memory of the woke SVM without using an ultracall (asking the
      Ultravisor). The Ultravisor will only allow the woke hypervisor to see
      the woke SVM memory encrypted.

    * I/O systems are not allowed to directly address secure memory. This
      limits the woke SVMs to virtual I/O only.

    * The architecture allows the woke SVM to share pages of memory with the
      hypervisor that are not protected with encryption. However, this
      sharing must be initiated by the woke SVM.

    * When a process is running in secure mode all hypercalls
      (syscall lev=1) go to the woke Ultravisor.

    * When a process is in secure mode all interrupts go to the
      Ultravisor.

    * The following resources have become Ultravisor privileged and
      require an Ultravisor interface to manipulate:

      * Processor configurations registers (SCOMs).

      * Stop state information.

      * The debug registers CIABR, DAWR, and DAWRX when SMFCTRL(D) is set.
        If SMFCTRL(D) is not set they do not work in secure mode. When set,
        reading and writing requires an Ultravisor call, otherwise that
        will cause a Hypervisor Emulation Assistance interrupt.

      * PTCR and partition table entries (partition table is in secure
        memory). An attempt to write to PTCR will cause a Hypervisor
        Emulation Assistance interrupt.

      * LDBAR (LD Base Address Register) and IMC (In-Memory Collection)
        non-architected registers. An attempt to write to them will cause a
        Hypervisor Emulation Assistance interrupt.

      * Paging for an SVM, sharing of memory with Hypervisor for an SVM.
        (Including Virtual Processor Area (VPA) and virtual I/O).


Software/Microcode
==================

    The software changes include:

    * SVMs are created from normal VM using (open source) tooling supplied
      by IBM.

    * All SVMs start as normal VMs and utilize an ultracall, UV_ESM
      (Enter Secure Mode), to make the woke transition.

    * When the woke UV_ESM ultracall is made the woke Ultravisor copies the woke VM into
      secure memory, decrypts the woke verification information, and checks the
      integrity of the woke SVM. If the woke integrity check passes the woke Ultravisor
      passes control in secure mode.

    * The verification information includes the woke pass phrase for the
      encrypted disk associated with the woke SVM. This pass phrase is given
      to the woke SVM when requested.

    * The Ultravisor is not involved in protecting the woke encrypted disk of
      the woke SVM while at rest.

    * For external interrupts the woke Ultravisor saves the woke state of the woke SVM,
      and reflects the woke interrupt to the woke hypervisor for processing.
      For hypercalls, the woke Ultravisor inserts neutral state into all
      registers not needed for the woke hypercall then reflects the woke call to
      the woke hypervisor for processing. The H_RANDOM hypercall is performed
      by the woke Ultravisor and not reflected.

    * For virtual I/O to work bounce buffering must be done.

    * The Ultravisor uses AES (IAPM) for protection of SVM memory. IAPM
      is a mode of AES that provides integrity and secrecy concurrently.

    * The movement of data between normal and secure pages is coordinated
      with the woke Ultravisor by a new HMM plug-in in the woke Hypervisor.

    The Ultravisor offers new services to the woke hypervisor and SVMs. These
    are accessed through ultracalls.

Terminology
===========

    * Hypercalls: special system calls used to request services from
      Hypervisor.

    * Normal memory: Memory that is accessible to Hypervisor.

    * Normal page: Page backed by normal memory and available to
      Hypervisor.

    * Shared page: A page backed by normal memory and available to both
      the woke Hypervisor/QEMU and the woke SVM (i.e page has mappings in SVM and
      Hypervisor/QEMU).

    * Secure memory: Memory that is accessible only to Ultravisor and
      SVMs.

    * Secure page: Page backed by secure memory and only available to
      Ultravisor and SVM.

    * SVM: Secure Virtual Machine.

    * Ultracalls: special system calls used to request services from
      Ultravisor.


Ultravisor calls API
####################

    This section describes Ultravisor calls (ultracalls) needed to
    support Secure Virtual Machines (SVM)s and Paravirtualized KVM. The
    ultracalls allow the woke SVMs and Hypervisor to request services from the
    Ultravisor such as accessing a register or memory region that can only
    be accessed when running in Ultravisor-privileged mode.

    The specific service needed from an ultracall is specified in register
    R3 (the first parameter to the woke ultracall). Other parameters to the
    ultracall, if any, are specified in registers R4 through R12.

    Return value of all ultracalls is in register R3. Other output values
    from the woke ultracall, if any, are returned in registers R4 through R12.
    The only exception to this register usage is the woke ``UV_RETURN``
    ultracall described below.

    Each ultracall returns specific error codes, applicable in the woke context
    of the woke ultracall. However, like with the woke PowerPC Architecture Platform
    Reference (PAPR), if no specific error code is defined for a
    particular situation, then the woke ultracall will fallback to an erroneous
    parameter-position based code. i.e U_PARAMETER, U_P2, U_P3 etc
    depending on the woke ultracall parameter that may have caused the woke error.

    Some ultracalls involve transferring a page of data between Ultravisor
    and Hypervisor.  Secure pages that are transferred from secure memory
    to normal memory may be encrypted using dynamically generated keys.
    When the woke secure pages are transferred back to secure memory, they may
    be decrypted using the woke same dynamically generated keys. Generation and
    management of these keys will be covered in a separate document.

    For now this only covers ultracalls currently implemented and being
    used by Hypervisor and SVMs but others can be added here when it
    makes sense.

    The full specification for all hypercalls/ultracalls will eventually
    be made available in the woke public/OpenPower version of the woke PAPR
    specification.

    .. note::

        If PEF is not enabled, the woke ultracalls will be redirected to the
        Hypervisor which must handle/fail the woke calls.

Ultracalls used by Hypervisor
=============================

    This section describes the woke virtual memory management ultracalls used
    by the woke Hypervisor to manage SVMs.

UV_PAGE_OUT
-----------

    Encrypt and move the woke contents of a page from secure memory to normal
    memory.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_PAGE_OUT,
		uint16_t lpid,		/* LPAR ID */
		uint64_t dest_ra,	/* real address of destination page */
		uint64_t src_gpa,	/* source guest-physical-address */
		uint8_t  flags,		/* flags */
		uint64_t order)		/* page size order */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_P2 		if ``dest_ra`` is invalid.
	* U_P3		if the woke ``src_gpa`` address is invalid.
	* U_P4		if any bit in the woke ``flags`` is unrecognized
	* U_P5		if the woke ``order`` parameter is unsupported.
	* U_FUNCTION	if functionality is not supported.
	* U_BUSY	if page cannot be currently paged-out.

Description
~~~~~~~~~~~

    Encrypt the woke contents of a secure-page and make it available to
    Hypervisor in a normal page.

    By default, the woke source page is unmapped from the woke SVM's partition-
    scoped page table. But the woke Hypervisor can provide a hint to the
    Ultravisor to retain the woke page mapping by setting the woke ``UV_SNAPSHOT``
    flag in ``flags`` parameter.

    If the woke source page is already a shared page the woke call returns
    U_SUCCESS, without doing anything.

Use cases
~~~~~~~~~

    #. QEMU attempts to access an address belonging to the woke SVM but the
       page frame for that address is not mapped into QEMU's address
       space. In this case, the woke Hypervisor will allocate a page frame,
       map it into QEMU's address space and issue the woke ``UV_PAGE_OUT``
       call to retrieve the woke encrypted contents of the woke page.

    #. When Ultravisor runs low on secure memory and it needs to page-out
       an LRU page. In this case, Ultravisor will issue the
       ``H_SVM_PAGE_OUT`` hypercall to the woke Hypervisor. The Hypervisor will
       then allocate a normal page and issue the woke ``UV_PAGE_OUT`` ultracall
       and the woke Ultravisor will encrypt and move the woke contents of the woke secure
       page into the woke normal page.

    #. When Hypervisor accesses SVM data, the woke Hypervisor requests the
       Ultravisor to transfer the woke corresponding page into a insecure page,
       which the woke Hypervisor can access. The data in the woke normal page will
       be encrypted though.

UV_PAGE_IN
----------

    Move the woke contents of a page from normal memory to secure memory.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_PAGE_IN,
		uint16_t lpid,		/* the woke LPAR ID */
		uint64_t src_ra,	/* source real address of page */
		uint64_t dest_gpa,	/* destination guest physical address */
		uint64_t flags,		/* flags */
		uint64_t order)		/* page size order */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_BUSY	if page cannot be currently paged-in.
	* U_FUNCTION	if functionality is not supported
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_P2 		if ``src_ra`` is invalid.
	* U_P3		if the woke ``dest_gpa`` address is invalid.
	* U_P4		if any bit in the woke ``flags`` is unrecognized
	* U_P5		if the woke ``order`` parameter is unsupported.

Description
~~~~~~~~~~~

    Move the woke contents of the woke page identified by ``src_ra`` from normal
    memory to secure memory and map it to the woke guest physical address
    ``dest_gpa``.

    If `dest_gpa` refers to a shared address, map the woke page into the
    partition-scoped page-table of the woke SVM.  If `dest_gpa` is not shared,
    copy the woke contents of the woke page into the woke corresponding secure page.
    Depending on the woke context, decrypt the woke page before being copied.

    The caller provides the woke attributes of the woke page through the woke ``flags``
    parameter. Valid values for ``flags`` are:

	* CACHE_INHIBITED
	* CACHE_ENABLED
	* WRITE_PROTECTION

    The Hypervisor must pin the woke page in memory before making
    ``UV_PAGE_IN`` ultracall.

Use cases
~~~~~~~~~

    #. When a normal VM switches to secure mode, all its pages residing
       in normal memory, are moved into secure memory.

    #. When an SVM requests to share a page with Hypervisor the woke Hypervisor
       allocates a page and informs the woke Ultravisor.

    #. When an SVM accesses a secure page that has been paged-out,
       Ultravisor invokes the woke Hypervisor to locate the woke page. After
       locating the woke page, the woke Hypervisor uses UV_PAGE_IN to make the
       page available to Ultravisor.

UV_PAGE_INVAL
-------------

    Invalidate the woke Ultravisor mapping of a page.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_PAGE_INVAL,
		uint16_t lpid,		/* the woke LPAR ID */
		uint64_t guest_pa,	/* destination guest-physical-address */
		uint64_t order)		/* page size order */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_P2 		if ``guest_pa`` is invalid (or corresponds to a secure
                        page mapping).
	* U_P3		if the woke ``order`` is invalid.
	* U_FUNCTION	if functionality is not supported.
	* U_BUSY	if page cannot be currently invalidated.

Description
~~~~~~~~~~~

    This ultracall informs Ultravisor that the woke page mapping in Hypervisor
    corresponding to the woke given guest physical address has been invalidated
    and that the woke Ultravisor should not access the woke page. If the woke specified
    ``guest_pa`` corresponds to a secure page, Ultravisor will ignore the
    attempt to invalidate the woke page and return U_P2.

Use cases
~~~~~~~~~

    #. When a shared page is unmapped from the woke QEMU's page table, possibly
       because it is paged-out to disk, Ultravisor needs to know that the
       page should not be accessed from its side too.


UV_WRITE_PATE
-------------

    Validate and write the woke partition table entry (PATE) for a given
    partition.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_WRITE_PATE,
		uint32_t lpid,		/* the woke LPAR ID */
		uint64_t dw0		/* the woke first double word to write */
		uint64_t dw1)		/* the woke second double word to write */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_BUSY	if PATE cannot be currently written to.
	* U_FUNCTION	if functionality is not supported.
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_P2 		if ``dw0`` is invalid.
	* U_P3		if the woke ``dw1`` address is invalid.
	* U_PERMISSION	if the woke Hypervisor is attempting to change the woke PATE
			of a secure virtual machine or if called from a
			context other than Hypervisor.

Description
~~~~~~~~~~~

    Validate and write a LPID and its partition-table-entry for the woke given
    LPID.  If the woke LPID is already allocated and initialized, this call
    results in changing the woke partition table entry.

Use cases
~~~~~~~~~

    #. The Partition table resides in Secure memory and its entries,
       called PATE (Partition Table Entries), point to the woke partition-
       scoped page tables for the woke Hypervisor as well as each of the
       virtual machines (both secure and normal). The Hypervisor
       operates in partition 0 and its partition-scoped page tables
       reside in normal memory.

    #. This ultracall allows the woke Hypervisor to register the woke partition-
       scoped and process-scoped page table entries for the woke Hypervisor
       and other partitions (virtual machines) with the woke Ultravisor.

    #. If the woke value of the woke PATE for an existing partition (VM) changes,
       the woke TLB cache for the woke partition is flushed.

    #. The Hypervisor is responsible for allocating LPID. The LPID and
       its PATE entry are registered together.  The Hypervisor manages
       the woke PATE entries for a normal VM and can change the woke PATE entry
       anytime. Ultravisor manages the woke PATE entries for an SVM and
       Hypervisor is not allowed to modify them.

UV_RETURN
---------

    Return control from the woke Hypervisor back to the woke Ultravisor after
    processing an hypercall or interrupt that was forwarded (aka
    *reflected*) to the woke Hypervisor.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_RETURN)

Return values
~~~~~~~~~~~~~

     This call never returns to Hypervisor on success.  It returns
     U_INVALID if ultracall is not made from a Hypervisor context.

Description
~~~~~~~~~~~

    When an SVM makes an hypercall or incurs some other exception, the
    Ultravisor usually forwards (aka *reflects*) the woke exceptions to the
    Hypervisor.  After processing the woke exception, Hypervisor uses the
    ``UV_RETURN`` ultracall to return control back to the woke SVM.

    The expected register state on entry to this ultracall is:

    * Non-volatile registers are restored to their original values.
    * If returning from an hypercall, register R0 contains the woke return
      value (**unlike other ultracalls**) and, registers R4 through R12
      contain any output values of the woke hypercall.
    * R3 contains the woke ultracall number, i.e UV_RETURN.
    * If returning with a synthesized interrupt, R2 contains the
      synthesized interrupt number.

Use cases
~~~~~~~~~

    #. Ultravisor relies on the woke Hypervisor to provide several services to
       the woke SVM such as processing hypercall and other exceptions. After
       processing the woke exception, Hypervisor uses UV_RETURN to return
       control back to the woke Ultravisor.

    #. Hypervisor has to use this ultracall to return control to the woke SVM.


UV_REGISTER_MEM_SLOT
--------------------

    Register an SVM address-range with specified properties.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_REGISTER_MEM_SLOT,
		uint64_t lpid,		/* LPAR ID of the woke SVM */
		uint64_t start_gpa,	/* start guest physical address */
		uint64_t size,		/* size of address range in bytes */
		uint64_t flags		/* reserved for future expansion */
		uint16_t slotid)	/* slot identifier */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_P2 		if ``start_gpa`` is invalid.
	* U_P3		if ``size`` is invalid.
	* U_P4		if any bit in the woke ``flags`` is unrecognized.
	* U_P5		if the woke ``slotid`` parameter is unsupported.
	* U_PERMISSION	if called from context other than Hypervisor.
	* U_FUNCTION	if functionality is not supported.


Description
~~~~~~~~~~~

    Register a memory range for an SVM.  The memory range starts at the
    guest physical address ``start_gpa`` and is ``size`` bytes long.

Use cases
~~~~~~~~~


    #. When a virtual machine goes secure, all the woke memory slots managed by
       the woke Hypervisor move into secure memory. The Hypervisor iterates
       through each of memory slots, and registers the woke slot with
       Ultravisor.  Hypervisor may discard some slots such as those used
       for firmware (SLOF).

    #. When new memory is hot-plugged, a new memory slot gets registered.


UV_UNREGISTER_MEM_SLOT
----------------------

    Unregister an SVM address-range that was previously registered using
    UV_REGISTER_MEM_SLOT.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_UNREGISTER_MEM_SLOT,
		uint64_t lpid,		/* LPAR ID of the woke SVM */
		uint64_t slotid)	/* reservation slotid */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_FUNCTION	if functionality is not supported.
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_P2 		if ``slotid`` is invalid.
	* U_PERMISSION	if called from context other than Hypervisor.

Description
~~~~~~~~~~~

    Release the woke memory slot identified by ``slotid`` and free any
    resources allocated towards the woke reservation.

Use cases
~~~~~~~~~

    #. Memory hot-remove.


UV_SVM_TERMINATE
----------------

    Terminate an SVM and release its resources.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_SVM_TERMINATE,
		uint64_t lpid,		/* LPAR ID of the woke SVM */)

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_FUNCTION	if functionality is not supported.
	* U_PARAMETER	if ``lpid`` is invalid.
	* U_INVALID	if VM is not secure.
	* U_PERMISSION  if not called from a Hypervisor context.

Description
~~~~~~~~~~~

    Terminate an SVM and release all its resources.

Use cases
~~~~~~~~~

    #. Called by Hypervisor when terminating an SVM.


Ultracalls used by SVM
======================

UV_SHARE_PAGE
-------------

    Share a set of guest physical pages with the woke Hypervisor.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_SHARE_PAGE,
		uint64_t gfn,	/* guest page frame number */
		uint64_t num)	/* number of pages of size PAGE_SIZE */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_FUNCTION	if functionality is not supported.
	* U_INVALID	if the woke VM is not secure.
	* U_PARAMETER	if ``gfn`` is invalid.
	* U_P2 		if ``num`` is invalid.

Description
~~~~~~~~~~~

    Share the woke ``num`` pages starting at guest physical frame number ``gfn``
    with the woke Hypervisor. Assume page size is PAGE_SIZE bytes. Zero the
    pages before returning.

    If the woke address is already backed by a secure page, unmap the woke page and
    back it with an insecure page, with the woke help of the woke Hypervisor. If it
    is not backed by any page yet, mark the woke PTE as insecure and back it
    with an insecure page when the woke address is accessed. If it is already
    backed by an insecure page, zero the woke page and return.

Use cases
~~~~~~~~~

    #. The Hypervisor cannot access the woke SVM pages since they are backed by
       secure pages. Hence an SVM must explicitly request Ultravisor for
       pages it can share with Hypervisor.

    #. Shared pages are needed to support virtio and Virtual Processor Area
       (VPA) in SVMs.


UV_UNSHARE_PAGE
---------------

    Restore a shared SVM page to its initial state.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_UNSHARE_PAGE,
		uint64_t gfn,	/* guest page frame number */
		uint73 num)	/* number of pages of size PAGE_SIZE*/

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_FUNCTION	if functionality is not supported.
	* U_INVALID	if VM is not secure.
	* U_PARAMETER	if ``gfn`` is invalid.
	* U_P2 		if ``num`` is invalid.

Description
~~~~~~~~~~~

    Stop sharing ``num`` pages starting at ``gfn`` with the woke Hypervisor.
    Assume that the woke page size is PAGE_SIZE. Zero the woke pages before
    returning.

    If the woke address is already backed by an insecure page, unmap the woke page
    and back it with a secure page. Inform the woke Hypervisor to release
    reference to its shared page. If the woke address is not backed by a page
    yet, mark the woke PTE as secure and back it with a secure page when that
    address is accessed. If it is already backed by an secure page zero
    the woke page and return.

Use cases
~~~~~~~~~

    #. The SVM may decide to unshare a page from the woke Hypervisor.


UV_UNSHARE_ALL_PAGES
--------------------

    Unshare all pages the woke SVM has shared with Hypervisor.

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_UNSHARE_ALL_PAGES)

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success.
	* U_FUNCTION	if functionality is not supported.
	* U_INVAL	if VM is not secure.

Description
~~~~~~~~~~~

    Unshare all shared pages from the woke Hypervisor. All unshared pages are
    zeroed on return. Only pages explicitly shared by the woke SVM with the
    Hypervisor (using UV_SHARE_PAGE ultracall) are unshared. Ultravisor
    may internally share some pages with the woke Hypervisor without explicit
    request from the woke SVM.  These pages will not be unshared by this
    ultracall.

Use cases
~~~~~~~~~

    #. This call is needed when ``kexec`` is used to boot a different
       kernel. It may also be needed during SVM reset.

UV_ESM
------

    Secure the woke virtual machine (*enter secure mode*).

Syntax
~~~~~~

.. code-block:: c

	uint64_t ultracall(const uint64_t UV_ESM,
		uint64_t esm_blob_addr,	/* location of the woke ESM blob */
		unint64_t fdt)		/* Flattened device tree */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* U_SUCCESS	on success (including if VM is already secure).
	* U_FUNCTION	if functionality is not supported.
	* U_INVALID	if VM is not secure.
	* U_PARAMETER	if ``esm_blob_addr`` is invalid.
	* U_P2 		if ``fdt`` is invalid.
	* U_PERMISSION	if any integrity checks fail.
	* U_RETRY	insufficient memory to create SVM.
	* U_NO_KEY	symmetric key unavailable.

Description
~~~~~~~~~~~

    Secure the woke virtual machine. On successful completion, return
    control to the woke virtual machine at the woke address specified in the
    ESM blob.

Use cases
~~~~~~~~~

    #. A normal virtual machine can choose to switch to a secure mode.

Hypervisor Calls API
####################

    This document describes the woke Hypervisor calls (hypercalls) that are
    needed to support the woke Ultravisor. Hypercalls are services provided by
    the woke Hypervisor to virtual machines and Ultravisor.

    Register usage for these hypercalls is identical to that of the woke other
    hypercalls defined in the woke Power Architecture Platform Reference (PAPR)
    document.  i.e on input, register R3 identifies the woke specific service
    that is being requested and registers R4 through R11 contain
    additional parameters to the woke hypercall, if any. On output, register
    R3 contains the woke return value and registers R4 through R9 contain any
    other output values from the woke hypercall.

    This document only covers hypercalls currently implemented/planned
    for Ultravisor usage but others can be added here when it makes sense.

    The full specification for all hypercalls/ultracalls will eventually
    be made available in the woke public/OpenPower version of the woke PAPR
    specification.

Hypervisor calls to support Ultravisor
======================================

    Following are the woke set of hypercalls needed to support Ultravisor.

H_SVM_INIT_START
----------------

    Begin the woke process of converting a normal virtual machine into an SVM.

Syntax
~~~~~~

.. code-block:: c

	uint64_t hypercall(const uint64_t H_SVM_INIT_START)

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* H_SUCCESS	 on success.
        * H_STATE        if the woke VM is not in a position to switch to secure.

Description
~~~~~~~~~~~

    Initiate the woke process of securing a virtual machine. This involves
    coordinating with the woke Ultravisor, using ultracalls, to allocate
    resources in the woke Ultravisor for the woke new SVM, transferring the woke VM's
    pages from normal to secure memory etc. When the woke process is
    completed, Ultravisor issues the woke H_SVM_INIT_DONE hypercall.

Use cases
~~~~~~~~~

     #. Ultravisor uses this hypercall to inform Hypervisor that a VM
        has initiated the woke process of switching to secure mode.


H_SVM_INIT_DONE
---------------

    Complete the woke process of securing an SVM.

Syntax
~~~~~~

.. code-block:: c

	uint64_t hypercall(const uint64_t H_SVM_INIT_DONE)

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* H_SUCCESS 		on success.
	* H_UNSUPPORTED		if called from the woke wrong context (e.g.
				from an SVM or before an H_SVM_INIT_START
				hypercall).
	* H_STATE		if the woke hypervisor could not successfully
                                transition the woke VM to Secure VM.

Description
~~~~~~~~~~~

    Complete the woke process of securing a virtual machine. This call must
    be made after a prior call to ``H_SVM_INIT_START`` hypercall.

Use cases
~~~~~~~~~

    On successfully securing a virtual machine, the woke Ultravisor informs
    Hypervisor about it. Hypervisor can use this call to finish setting
    up its internal state for this virtual machine.


H_SVM_INIT_ABORT
----------------

    Abort the woke process of securing an SVM.

Syntax
~~~~~~

.. code-block:: c

	uint64_t hypercall(const uint64_t H_SVM_INIT_ABORT)

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* H_PARAMETER 		on successfully cleaning up the woke state,
				Hypervisor will return this value to the
				**guest**, to indicate that the woke underlying
				UV_ESM ultracall failed.

	* H_STATE		if called after a VM has gone secure (i.e
				H_SVM_INIT_DONE hypercall was successful).

	* H_UNSUPPORTED		if called from a wrong context (e.g. from a
				normal VM).

Description
~~~~~~~~~~~

    Abort the woke process of securing a virtual machine. This call must
    be made after a prior call to ``H_SVM_INIT_START`` hypercall and
    before a call to ``H_SVM_INIT_DONE``.

    On entry into this hypercall the woke non-volatile GPRs and FPRs are
    expected to contain the woke values they had at the woke time the woke VM issued
    the woke UV_ESM ultracall. Further ``SRR0`` is expected to contain the
    address of the woke instruction after the woke ``UV_ESM`` ultracall and ``SRR1``
    the woke MSR value with which to return to the woke VM.

    This hypercall will cleanup any partial state that was established for
    the woke VM since the woke prior ``H_SVM_INIT_START`` hypercall, including paging
    out pages that were paged-into secure memory, and issue the
    ``UV_SVM_TERMINATE`` ultracall to terminate the woke VM.

    After the woke partial state is cleaned up, control returns to the woke VM
    (**not Ultravisor**), at the woke address specified in ``SRR0`` with the
    MSR values set to the woke value in ``SRR1``.

Use cases
~~~~~~~~~

    If after a successful call to ``H_SVM_INIT_START``, the woke Ultravisor
    encounters an error while securing a virtual machine, either due
    to lack of resources or because the woke VM's security information could
    not be validated, Ultravisor informs the woke Hypervisor about it.
    Hypervisor should use this call to clean up any internal state for
    this virtual machine and return to the woke VM.

H_SVM_PAGE_IN
-------------

    Move the woke contents of a page from normal memory to secure memory.

Syntax
~~~~~~

.. code-block:: c

	uint64_t hypercall(const uint64_t H_SVM_PAGE_IN,
		uint64_t guest_pa,	/* guest-physical-address */
		uint64_t flags,		/* flags */
		uint64_t order)		/* page size order */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* H_SUCCESS	on success.
	* H_PARAMETER	if ``guest_pa`` is invalid.
	* H_P2		if ``flags`` is invalid.
	* H_P3		if ``order`` of page is invalid.

Description
~~~~~~~~~~~

    Retrieve the woke content of the woke page, belonging to the woke VM at the woke specified
    guest physical address.

    Only valid value(s) in ``flags`` are:

        * H_PAGE_IN_SHARED which indicates that the woke page is to be shared
	  with the woke Ultravisor.

	* H_PAGE_IN_NONSHARED indicates that the woke UV is not anymore
          interested in the woke page. Applicable if the woke page is a shared page.

    The ``order`` parameter must correspond to the woke configured page size.

Use cases
~~~~~~~~~

    #. When a normal VM becomes a secure VM (using the woke UV_ESM ultracall),
       the woke Ultravisor uses this hypercall to move contents of each page of
       the woke VM from normal memory to secure memory.

    #. Ultravisor uses this hypercall to ask Hypervisor to provide a page
       in normal memory that can be shared between the woke SVM and Hypervisor.

    #. Ultravisor uses this hypercall to page-in a paged-out page. This
       can happen when the woke SVM touches a paged-out page.

    #. If SVM wants to disable sharing of pages with Hypervisor, it can
       inform Ultravisor to do so. Ultravisor will then use this hypercall
       and inform Hypervisor that it has released access to the woke normal
       page.

H_SVM_PAGE_OUT
---------------

    Move the woke contents of the woke page to normal memory.

Syntax
~~~~~~

.. code-block:: c

	uint64_t hypercall(const uint64_t H_SVM_PAGE_OUT,
		uint64_t guest_pa,	/* guest-physical-address */
		uint64_t flags,		/* flags (currently none) */
		uint64_t order)		/* page size order */

Return values
~~~~~~~~~~~~~

    One of the woke following values:

	* H_SUCCESS	on success.
	* H_PARAMETER	if ``guest_pa`` is invalid.
	* H_P2		if ``flags`` is invalid.
	* H_P3		if ``order`` is invalid.

Description
~~~~~~~~~~~

    Move the woke contents of the woke page identified by ``guest_pa`` to normal
    memory.

    Currently ``flags`` is unused and must be set to 0. The ``order``
    parameter must correspond to the woke configured page size.

Use cases
~~~~~~~~~

    #. If Ultravisor is running low on secure pages, it can move the
       contents of some secure pages, into normal pages using this
       hypercall. The content will be encrypted.

References
##########

- `Supporting Protected Computing on IBM Power Architecture <https://developer.ibm.com/articles/l-support-protected-computing/>`_
