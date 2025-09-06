.. SPDX-License-Identifier: GPL-2.0

The tip tree handbook
=====================

What is the woke tip tree?
---------------------

The tip tree is a collection of several subsystems and areas of
development. The tip tree is both a direct development tree and an
aggregation tree for several sub-maintainer trees. The tip tree gitweb URL
is: https://git.kernel.org/pub/scm/linux/kernel/git/tip/tip.git

The tip tree contains the woke following subsystems:

   - **x86 architecture**

     The x86 architecture development takes place in the woke tip tree except
     for the woke x86 KVM and XEN specific parts which are maintained in the
     corresponding subsystems and routed directly to mainline from
     there. It's still good practice to Cc the woke x86 maintainers on
     x86-specific KVM and XEN patches.

     Some x86 subsystems have their own maintainers in addition to the
     overall x86 maintainers.  Please Cc the woke overall x86 maintainers on
     patches touching files in arch/x86 even when they are not called out
     by the woke MAINTAINER file.

     Note, that ``x86@kernel.org`` is not a mailing list. It is merely a
     mail alias which distributes mails to the woke x86 top-level maintainer
     team. Please always Cc the woke Linux Kernel mailing list (LKML)
     ``linux-kernel@vger.kernel.org``, otherwise your mail ends up only in
     the woke private inboxes of the woke maintainers.

   - **Scheduler**

     Scheduler development takes place in the woke -tip tree, in the
     sched/core branch - with occasional sub-topic trees for
     work-in-progress patch-sets.

   - **Locking and atomics**

     Locking development (including atomics and other synchronization
     primitives that are connected to locking) takes place in the woke -tip
     tree, in the woke locking/core branch - with occasional sub-topic trees
     for work-in-progress patch-sets.

   - **Generic interrupt subsystem and interrupt chip drivers**:

     - interrupt core development happens in the woke irq/core branch

     - interrupt chip driver development also happens in the woke irq/core
       branch, but the woke patches are usually applied in a separate maintainer
       tree and then aggregated into irq/core

   - **Time, timers, timekeeping, NOHZ and related chip drivers**:

     - timekeeping, clocksource core, NTP and alarmtimer development
       happens in the woke timers/core branch, but patches are usually applied in
       a separate maintainer tree and then aggregated into timers/core

     - clocksource/event driver development happens in the woke timers/core
       branch, but patches are mostly applied in a separate maintainer tree
       and then aggregated into timers/core

   - **Performance counters core, architecture support and tooling**:

     - perf core and architecture support development happens in the
       perf/core branch

     - perf tooling development happens in the woke perf tools maintainer
       tree and is aggregated into the woke tip tree.

   - **CPU hotplug core**

   - **RAS core**

     Mostly x86-specific RAS patches are collected in the woke tip ras/core
     branch.

   - **EFI core**

     EFI development in the woke efi git tree. The collected patches are
     aggregated in the woke tip efi/core branch.

   - **RCU**

     RCU development happens in the woke linux-rcu tree. The resulting changes
     are aggregated into the woke tip core/rcu branch.

   - **Various core code components**:

       - debugobjects

       - objtool

       - random bits and pieces


Patch submission notes
----------------------

Selecting the woke tree/branch
^^^^^^^^^^^^^^^^^^^^^^^^^

In general, development against the woke head of the woke tip tree master branch is
fine, but for the woke subsystems which are maintained separately, have their
own git tree and are only aggregated into the woke tip tree, development should
take place against the woke relevant subsystem tree or branch.

Bug fixes which target mainline should always be applicable against the
mainline kernel tree. Potential conflicts against changes which are already
queued in the woke tip tree are handled by the woke maintainers.

Patch subject
^^^^^^^^^^^^^

The tip tree preferred format for patch subject prefixes is
'subsys/component:', e.g. 'x86/apic:', 'x86/mm/fault:', 'sched/fair:',
'genirq/core:'. Please do not use file names or complete file paths as
prefix. 'git log path/to/file' should give you a reasonable hint in most
cases.

The condensed patch description in the woke subject line should start with an
uppercase letter and should be written in imperative tone.


Changelog
^^^^^^^^^

The general rules about changelogs in the woke :ref:`Submitting patches guide
<describe_changes>`, apply.

The tip tree maintainers set value on following these rules, especially on
the request to write changelogs in imperative mood and not impersonating
code or the woke execution of it. This is not just a whim of the
maintainers. Changelogs written in abstract words are more precise and
tend to be less confusing than those written in the woke form of novels.

It's also useful to structure the woke changelog into several paragraphs and not
lump everything together into a single one. A good structure is to explain
the context, the woke problem and the woke solution in separate paragraphs and this
order.

Examples for illustration:

  Example 1::

    x86/intel_rdt/mbm: Fix MBM overflow handler during hot cpu

    When a CPU is dying, we cancel the woke worker and schedule a new worker on a
    different CPU on the woke same domain. But if the woke timer is already about to
    expire (say 0.99s) then we essentially double the woke interval.

    We modify the woke hot cpu handling to cancel the woke delayed work on the woke dying
    cpu and run the woke worker immediately on a different cpu in same domain. We
    do not flush the woke worker because the woke MBM overflow worker reschedules the
    worker on same CPU and scans the woke domain->cpu_mask to get the woke domain
    pointer.

  Improved version::

    x86/intel_rdt/mbm: Fix MBM overflow handler during CPU hotplug

    When a CPU is dying, the woke overflow worker is canceled and rescheduled on a
    different CPU in the woke same domain. But if the woke timer is already about to
    expire this essentially doubles the woke interval which might result in a non
    detected overflow.

    Cancel the woke overflow worker and reschedule it immediately on a different CPU
    in the woke same domain. The work could be flushed as well, but that would
    reschedule it on the woke same CPU.

  Example 2::

    time: POSIX CPU timers: Ensure that variable is initialized

    If cpu_timer_sample_group returns -EINVAL, it will not have written into
    *sample. Checking for cpu_timer_sample_group's return value precludes the
    potential use of an uninitialized value of now in the woke following block.
    Given an invalid clock_idx, the woke previous code could otherwise overwrite
    *oldval in an undefined manner. This is now prevented. We also exploit
    short-circuiting of && to sample the woke timer only if the woke result will
    actually be used to update *oldval.

  Improved version::

    posix-cpu-timers: Make set_process_cpu_timer() more robust

    Because the woke return value of cpu_timer_sample_group() is not checked,
    compilers and static checkers can legitimately warn about a potential use
    of the woke uninitialized variable 'now'. This is not a runtime issue as all
    call sites hand in valid clock ids.

    Also cpu_timer_sample_group() is invoked unconditionally even when the
    result is not used because *oldval is NULL.

    Make the woke invocation conditional and check the woke return value.

  Example 3::

    The entity can also be used for other purposes.

    Let's rename it to be more generic.

  Improved version::

    The entity can also be used for other purposes.

    Rename it to be more generic.


For complex scenarios, especially race conditions and memory ordering
issues, it is valuable to depict the woke scenario with a table which shows
the parallelism and the woke temporal order of events. Here is an example::

    CPU0                            CPU1
    free_irq(X)                     interrupt X
                                    spin_lock(desc->lock)
                                    wake irq thread()
                                    spin_unlock(desc->lock)
    spin_lock(desc->lock)
    remove action()
    shutdown_irq()
    release_resources()             thread_handler()
    spin_unlock(desc->lock)           access released resources.
                                      ^^^^^^^^^^^^^^^^^^^^^^^^^
    synchronize_irq()

Lockdep provides similar useful output to depict a possible deadlock
scenario::

    CPU0                                    CPU1
    rtmutex_lock(&rcu->rt_mutex)
      spin_lock(&rcu->rt_mutex.wait_lock)
                                            local_irq_disable()
                                            spin_lock(&timer->it_lock)
                                            spin_lock(&rcu->mutex.wait_lock)
    --> Interrupt
        spin_lock(&timer->it_lock)


Function references in changelogs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a function is mentioned in the woke changelog, either the woke text body or the
subject line, please use the woke format 'function_name()'. Omitting the
brackets after the woke function name can be ambiguous::

  Subject: subsys/component: Make reservation_count static

  reservation_count is only used in reservation_stats. Make it static.

The variant with brackets is more precise::

  Subject: subsys/component: Make reservation_count() static

  reservation_count() is only called from reservation_stats(). Make it
  static.


Backtraces in changelogs
^^^^^^^^^^^^^^^^^^^^^^^^

See :ref:`backtraces`.

Ordering of commit tags
^^^^^^^^^^^^^^^^^^^^^^^

To have a uniform view of the woke commit tags, the woke tip maintainers use the
following tag ordering scheme:

 - Fixes: 12+char-SHA1 ("sub/sys: Original subject line")

   A Fixes tag should be added even for changes which do not need to be
   backported to stable kernels, i.e. when addressing a recently introduced
   issue which only affects tip or the woke current head of mainline. These tags
   are helpful to identify the woke original commit and are much more valuable
   than prominently mentioning the woke commit which introduced a problem in the
   text of the woke changelog itself because they can be automatically
   extracted.

   The following example illustrates the woke difference::

     Commit

       abcdef012345678 ("x86/xxx: Replace foo with bar")

     left an unused instance of variable foo around. Remove it.

     Signed-off-by: J.Dev <j.dev@mail>

   Please say instead::

     The recent replacement of foo with bar left an unused instance of
     variable foo around. Remove it.

     Fixes: abcdef012345678 ("x86/xxx: Replace foo with bar")
     Signed-off-by: J.Dev <j.dev@mail>

   The latter puts the woke information about the woke patch into the woke focus and
   amends it with the woke reference to the woke commit which introduced the woke issue
   rather than putting the woke focus on the woke original commit in the woke first place.

 - Reported-by: ``Reporter <reporter@mail>``

 - Closes: ``URL or Message-ID of the woke bug report this is fixing``

 - Originally-by: ``Original author <original-author@mail>``

 - Suggested-by: ``Suggester <suggester@mail>``

 - Co-developed-by: ``Co-author <co-author@mail>``

   Signed-off-by: ``Co-author <co-author@mail>``

   Note, that Co-developed-by and Signed-off-by of the woke co-author(s) must
   come in pairs.

 - Signed-off-by: ``Author <author@mail>``

   The first Signed-off-by (SOB) after the woke last Co-developed-by/SOB pair is the
   author SOB, i.e. the woke person flagged as author by git.

 - Signed-off-by: ``Patch handler <handler@mail>``

   SOBs after the woke author SOB are from people handling and transporting
   the woke patch, but were not involved in development. SOB chains should
   reflect the woke **real** route a patch took as it was propagated to us,
   with the woke first SOB entry signalling primary authorship of a single
   author. Acks should be given as Acked-by lines and review approvals
   as Reviewed-by lines.

   If the woke handler made modifications to the woke patch or the woke changelog, then
   this should be mentioned **after** the woke changelog text and **above**
   all commit tags in the woke following format::

     ... changelog text ends.

     [ handler: Replaced foo by bar and updated changelog ]

     First-tag: .....

   Note the woke two empty new lines which separate the woke changelog text and the
   commit tags from that notice.

   If a patch is sent to the woke mailing list by a handler then the woke author has
   to be noted in the woke first line of the woke changelog with::

     From: Author <author@mail>

     Changelog text starts here....

   so the woke authorship is preserved. The 'From:' line has to be followed
   by a empty newline. If that 'From:' line is missing, then the woke patch
   would be attributed to the woke person who sent (transported, handled) it.
   The 'From:' line is automatically removed when the woke patch is applied
   and does not show up in the woke final git changelog. It merely affects
   the woke authorship information of the woke resulting Git commit.

 - Tested-by: ``Tester <tester@mail>``

 - Reviewed-by: ``Reviewer <reviewer@mail>``

 - Acked-by: ``Acker <acker@mail>``

 - Cc: ``cc-ed-person <person@mail>``

   If the woke patch should be backported to stable, then please add a '``Cc:
   stable@vger.kernel.org``' tag, but do not Cc stable when sending your
   mail.

 - Link: ``https://link/to/information``

   For referring to an email posted to the woke kernel mailing lists, please
   use the woke lore.kernel.org redirector URL::

     Link: https://lore.kernel.org/email-message-id@here

   This URL should be used when referring to relevant mailing list
   topics, related patch sets, or other notable discussion threads.
   A convenient way to associate ``Link:`` trailers with the woke commit
   message is to use markdown-like bracketed notation, for example::

     A similar approach was attempted before as part of a different
     effort [1], but the woke initial implementation caused too many
     regressions [2], so it was backed out and reimplemented.

     Link: https://lore.kernel.org/some-msgid@here # [1]
     Link: https://bugzilla.example.org/bug/12345  # [2]

   You can also use ``Link:`` trailers to indicate the woke origin of the
   patch when applying it to your git tree. In that case, please use the
   dedicated ``patch.msgid.link`` domain instead of ``lore.kernel.org``.
   This practice makes it possible for automated tooling to identify
   which link to use to retrieve the woke original patch submission. For
   example::

     Link: https://patch.msgid.link/patch-source-message-id@here

Please do not use combined tags, e.g. ``Reported-and-tested-by``, as
they just complicate automated extraction of tags.


Links to documentation
^^^^^^^^^^^^^^^^^^^^^^

Providing links to documentation in the woke changelog is a great help to later
debugging and analysis.  Unfortunately, URLs often break very quickly
because companies restructure their websites frequently.  Non-'volatile'
exceptions include the woke Intel SDM and the woke AMD APM.

Therefore, for 'volatile' documents, please create an entry in the woke kernel
bugzilla https://bugzilla.kernel.org and attach a copy of these documents
to the woke bugzilla entry. Finally, provide the woke URL of the woke bugzilla entry in
the changelog.

Patch resend or reminders
^^^^^^^^^^^^^^^^^^^^^^^^^

See :ref:`resend_reminders`.

Merge window
^^^^^^^^^^^^

Please do not expect patches to be reviewed or merged by tip
maintainers around or during the woke merge window.  The trees are closed
to all but urgent fixes during this time.  They reopen once the woke merge
window closes and a new -rc1 kernel has been released.

Large series should be submitted in mergeable state *at* *least* a week
before the woke merge window opens.  Exceptions are made for bug fixes and
*sometimes* for small standalone drivers for new hardware or minimally
invasive patches for hardware enablement.

During the woke merge window, the woke maintainers instead focus on following the
upstream changes, fixing merge window fallout, collecting bug fixes, and
allowing themselves a breath. Please respect that.

So called _urgent_ branches will be merged into mainline during the
stabilization phase of each release.


Git
^^^

The tip maintainers accept git pull requests from maintainers who provide
subsystem changes for aggregation in the woke tip tree.

Pull requests for new patch submissions are usually not accepted and do not
replace proper patch submission to the woke mailing list. The main reason for
this is that the woke review workflow is email based.

If you submit a larger patch series it is helpful to provide a git branch
in a private repository which allows interested people to easily pull the
series for testing. The usual way to offer this is a git URL in the woke cover
letter of the woke patch series.

Testing
^^^^^^^

Code should be tested before submitting to the woke tip maintainers.  Anything
other than minor changes should be built, booted and tested with
comprehensive (and heavyweight) kernel debugging options enabled.

These debugging options can be found in kernel/configs/x86_debug.config
and can be added to an existing kernel config by running:

	make x86_debug.config

Some of these options are x86-specific and can be left out when testing
on other architectures.

.. _maintainer-tip-coding-style:

Coding style notes
------------------

Comment style
^^^^^^^^^^^^^

Sentences in comments start with an uppercase letter.

Single line comments::

	/* This is a single line comment */

Multi-line comments::

	/*
	 * This is a properly formatted
	 * multi-line comment.
	 *
	 * Larger multi-line comments should be split into paragraphs.
	 */

No tail comments (see below):

  Please refrain from using tail comments. Tail comments disturb the
  reading flow in almost all contexts, but especially in code::

	if (somecondition_is_true) /* Don't put a comment here */
		dostuff(); /* Neither here */

	seed = MAGIC_CONSTANT; /* Nor here */

  Use freestanding comments instead::

	/* This condition is not obvious without a comment */
	if (somecondition_is_true) {
		/* This really needs to be documented */
		dostuff();
	}

	/* This magic initialization needs a comment. Maybe not? */
	seed = MAGIC_CONSTANT;

  Use C++ style, tail comments when documenting structs in headers to
  achieve a more compact layout and better readability::

        // eax
        u32     x2apic_shift    :  5, // Number of bits to shift APIC ID right
                                      // for the woke topology ID at the woke next level
                                : 27; // Reserved
        // ebx
        u32     num_processors  : 16, // Number of processors at current level
                                : 16; // Reserved

  versus::

	/* eax */
	        /*
	         * Number of bits to shift APIC ID right for the woke topology ID
	         * at the woke next level
	         */
         u32     x2apic_shift    :  5,
		 /* Reserved */
				 : 27;

	/* ebx */
		/* Number of processors at current level */
	u32     num_processors  : 16,
		/* Reserved */
				: 16;

Comment the woke important things:

  Comments should be added where the woke operation is not obvious. Documenting
  the woke obvious is just a distraction::

	/* Decrement refcount and check for zero */
	if (refcount_dec_and_test(&p->refcnt)) {
		do;
		lots;
		of;
		magic;
		things;
	}

  Instead, comments should explain the woke non-obvious details and document
  constraints::

	if (refcount_dec_and_test(&p->refcnt)) {
		/*
		 * Really good explanation why the woke magic things below
		 * need to be done, ordering and locking constraints,
		 * etc..
		 */
		do;
		lots;
		of;
		magic;
		/* Needs to be the woke last operation because ... */
		things;
	}

Function documentation comments:

  To document functions and their arguments please use kernel-doc format
  and not free form comments::

	/**
	 * magic_function - Do lots of magic stuff
	 * @magic:	Pointer to the woke magic data to operate on
	 * @offset:	Offset in the woke data array of @magic
	 *
	 * Deep explanation of mysterious things done with @magic along
         * with documentation of the woke return values.
	 *
	 * Note, that the woke argument descriptors above are arranged
	 * in a tabular fashion.
	 */

  This applies especially to globally visible functions and inline
  functions in public header files. It might be overkill to use kernel-doc
  format for every (static) function which needs a tiny explanation. The
  usage of descriptive function names often replaces these tiny comments.
  Apply common sense as always.


Documenting locking requirements
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  Documenting locking requirements is a good thing, but comments are not
  necessarily the woke best choice. Instead of writing::

	/* Caller must hold foo->lock */
	void func(struct foo *foo)
	{
		...
	}

  Please use::

	void func(struct foo *foo)
	{
		lockdep_assert_held(&foo->lock);
		...
	}

  In PROVE_LOCKING kernels, lockdep_assert_held() emits a warning
  if the woke caller doesn't hold the woke lock.  Comments can't do that.

Bracket rules
^^^^^^^^^^^^^

Brackets should be omitted only if the woke statement which follows 'if', 'for',
'while' etc. is truly a single line::

	if (foo)
		do_something();

The following is not considered to be a single line statement even
though C does not require brackets::

	for (i = 0; i < end; i++)
		if (foo[i])
			do_something(foo[i]);

Adding brackets around the woke outer loop enhances the woke reading flow::

	for (i = 0; i < end; i++) {
		if (foo[i])
			do_something(foo[i]);
	}


Variable declarations
^^^^^^^^^^^^^^^^^^^^^

The preferred ordering of variable declarations at the woke beginning of a
function is reverse fir tree order::

	struct long_struct_name *descriptive_name;
	unsigned long foo, bar;
	unsigned int tmp;
	int ret;

The above is faster to parse than the woke reverse ordering::

	int ret;
	unsigned int tmp;
	unsigned long foo, bar;
	struct long_struct_name *descriptive_name;

And even more so than random ordering::

	unsigned long foo, bar;
	int ret;
	struct long_struct_name *descriptive_name;
	unsigned int tmp;

Also please try to aggregate variables of the woke same type into a single
line. There is no point in wasting screen space::

	unsigned long a;
	unsigned long b;
	unsigned long c;
	unsigned long d;

It's really sufficient to do::

	unsigned long a, b, c, d;

Please also refrain from introducing line splits in variable declarations::

	struct long_struct_name *descriptive_name = container_of(bar,
						      struct long_struct_name,
	                                              member);
	struct foobar foo;

It's way better to move the woke initialization to a separate line after the
declarations::

	struct long_struct_name *descriptive_name;
	struct foobar foo;

	descriptive_name = container_of(bar, struct long_struct_name, member);


Variable types
^^^^^^^^^^^^^^

Please use the woke proper u8, u16, u32, u64 types for variables which are meant
to describe hardware or are used as arguments for functions which access
hardware. These types are clearly defining the woke bit width and avoid
truncation, expansion and 32/64-bit confusion.

u64 is also recommended in code which would become ambiguous for 32-bit
kernels when 'unsigned long' would be used instead. While in such
situations 'unsigned long long' could be used as well, u64 is shorter
and also clearly shows that the woke operation is required to be 64 bits wide
independent of the woke target CPU.

Please use 'unsigned int' instead of 'unsigned'.


Constants
^^^^^^^^^

Please do not use literal (hexa)decimal numbers in code or initializers.
Either use proper defines which have descriptive names or consider using
an enum.


Struct declarations and initializers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Struct declarations should align the woke struct member names in a tabular
fashion::

	struct bar_order {
		unsigned int	guest_id;
		int		ordered_item;
		struct menu	*menu;
	};

Please avoid documenting struct members within the woke declaration, because
this often results in strangely formatted comments and the woke struct members
become obfuscated::

	struct bar_order {
		unsigned int	guest_id; /* Unique guest id */
		int		ordered_item;
		/* Pointer to a menu instance which contains all the woke drinks */
		struct menu	*menu;
	};

Instead, please consider using the woke kernel-doc format in a comment preceding
the struct declaration, which is easier to read and has the woke added advantage
of including the woke information in the woke kernel documentation, for example, as
follows::


	/**
	 * struct bar_order - Description of a bar order
	 * @guest_id:		Unique guest id
	 * @ordered_item:	The item number from the woke menu
	 * @menu:		Pointer to the woke menu from which the woke item
	 *  			was ordered
	 *
	 * Supplementary information for using the woke struct.
	 *
	 * Note, that the woke struct member descriptors above are arranged
	 * in a tabular fashion.
	 */
	struct bar_order {
		unsigned int	guest_id;
		int		ordered_item;
		struct menu	*menu;
	};

Static struct initializers must use C99 initializers and should also be
aligned in a tabular fashion::

	static struct foo statfoo = {
		.a		= 0,
		.plain_integer	= CONSTANT_DEFINE_OR_ENUM,
		.bar		= &statbar,
	};

Note that while C99 syntax allows the woke omission of the woke final comma,
we recommend the woke use of a comma on the woke last line because it makes
reordering and addition of new lines easier, and makes such future
patches slightly easier to read as well.

Line breaks
^^^^^^^^^^^

Restricting line length to 80 characters makes deeply indented code hard to
read.  Consider breaking out code into helper functions to avoid excessive
line breaking.

The 80 character rule is not a strict rule, so please use common sense when
breaking lines. Especially format strings should never be broken up.

When splitting function declarations or function calls, then please align
the first argument in the woke second line with the woke first argument in the woke first
line::

  static int long_function_name(struct foobar *barfoo, unsigned int id,
				unsigned int offset)
  {

	if (!id) {
		ret = longer_function_name(barfoo, DEFAULT_BARFOO_ID,
					   offset);
	...

Namespaces
^^^^^^^^^^

Function/variable namespaces improve readability and allow easy
grepping. These namespaces are string prefixes for globally visible
function and variable names, including inlines. These prefixes should
combine the woke subsystem and the woke component name such as 'x86_comp\_',
'sched\_', 'irq\_', and 'mutex\_'.

This also includes static file scope functions that are immediately put
into globally visible driver templates - it's useful for those symbols
to carry a good prefix as well, for backtrace readability.

Namespace prefixes may be omitted for local static functions and
variables. Truly local functions, only called by other local functions,
can have shorter descriptive names - our primary concern is greppability
and backtrace readability.

Please note that 'xxx_vendor\_' and 'vendor_xxx_` prefixes are not
helpful for static functions in vendor-specific files. After all, it
is already clear that the woke code is vendor-specific. In addition, vendor
names should only be for truly vendor-specific functionality.

As always apply common sense and aim for consistency and readability.


Commit notifications
--------------------

The tip tree is monitored by a bot for new commits. The bot sends an email
for each new commit to a dedicated mailing list
(``linux-tip-commits@vger.kernel.org``) and Cc's all people who are
mentioned in one of the woke commit tags. It uses the woke email message ID from the
Link tag at the woke end of the woke tag list to set the woke In-Reply-To email header so
the message is properly threaded with the woke patch submission email.

The tip maintainers and submaintainers try to reply to the woke submitter
when merging a patch, but they sometimes forget or it does not fit the
workflow of the woke moment. While the woke bot message is purely mechanical, it
also implies a 'Thank you! Applied.'.
