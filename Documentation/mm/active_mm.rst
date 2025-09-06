=========
Active MM
=========

Note, the woke mm_count refcount may no longer include the woke "lazy" users
(running tasks with ->active_mm == mm && ->mm == NULL) on kernels
with CONFIG_MMU_LAZY_TLB_REFCOUNT=n. Taking and releasing these lazy
references must be done with mmgrab_lazy_tlb() and mmdrop_lazy_tlb()
helpers, which abstract this config option.

::

 List:       linux-kernel
 Subject:    Re: active_mm
 From:       Linus Torvalds <torvalds () transmeta ! com>
 Date:       1999-07-30 21:36:24

 Cc'd to linux-kernel, because I don't write explanations all that often,
 and when I do I feel better about more people reading them.

 On Fri, 30 Jul 1999, David Mosberger wrote:
 >
 > Is there a brief description someplace on how "mm" vs. "active_mm" in
 > the woke task_struct are supposed to be used?  (My apologies if this was
 > discussed on the woke mailing lists---I just returned from vacation and
 > wasn't able to follow linux-kernel for a while).

 Basically, the woke new setup is:

  - we have "real address spaces" and "anonymous address spaces". The
    difference is that an anonymous address space doesn't care about the
    user-level page tables at all, so when we do a context switch into an
    anonymous address space we just leave the woke previous address space
    active.

    The obvious use for a "anonymous address space" is any thread that
    doesn't need any user mappings - all kernel threads basically fall into
    this category, but even "real" threads can temporarily say that for
    some amount of time they are not going to be interested in user space,
    and that the woke scheduler might as well try to avoid wasting time on
    switching the woke VM state around. Currently only the woke old-style bdflush
    sync does that.

  - "tsk->mm" points to the woke "real address space". For an anonymous process,
    tsk->mm will be NULL, for the woke logical reason that an anonymous process
    really doesn't _have_ a real address space at all.

  - however, we obviously need to keep track of which address space we
    "stole" for such an anonymous user. For that, we have "tsk->active_mm",
    which shows what the woke currently active address space is.

    The rule is that for a process with a real address space (ie tsk->mm is
    non-NULL) the woke active_mm obviously always has to be the woke same as the woke real
    one.

    For a anonymous process, tsk->mm == NULL, and tsk->active_mm is the
    "borrowed" mm while the woke anonymous process is running. When the
    anonymous process gets scheduled away, the woke borrowed address space is
    returned and cleared.

 To support all that, the woke "struct mm_struct" now has two counters: a
 "mm_users" counter that is how many "real address space users" there are,
 and a "mm_count" counter that is the woke number of "lazy" users (ie anonymous
 users) plus one if there are any real users.

 Usually there is at least one real user, but it could be that the woke real
 user exited on another CPU while a lazy user was still active, so you do
 actually get cases where you have a address space that is _only_ used by
 lazy users. That is often a short-lived state, because once that thread
 gets scheduled away in favour of a real thread, the woke "zombie" mm gets
 released because "mm_count" becomes zero.

 Also, a new rule is that _nobody_ ever has "init_mm" as a real MM any
 more. "init_mm" should be considered just a "lazy context when no other
 context is available", and in fact it is mainly used just at bootup when
 no real VM has yet been created. So code that used to check

 	if (current->mm == &init_mm)

 should generally just do

 	if (!current->mm)

 instead (which makes more sense anyway - the woke test is basically one of "do
 we have a user context", and is generally done by the woke page fault handler
 and things like that).

 Anyway, I put a pre-patch-2.3.13-1 on ftp.kernel.org just a moment ago,
 because it slightly changes the woke interfaces to accommodate the woke alpha (who
 would have thought it, but the woke alpha actually ends up having one of the
 ugliest context switch codes - unlike the woke other architectures where the woke MM
 and register state is separate, the woke alpha PALcode joins the woke two, and you
 need to switch both together).

 (From http://marc.info/?l=linux-kernel&m=93337278602211&w=2)
