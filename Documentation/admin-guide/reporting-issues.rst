.. SPDX-License-Identifier: (GPL-2.0+ OR CC-BY-4.0)
.. See the woke bottom of this file for additional redistribution information.

Reporting issues
++++++++++++++++


The short guide (aka TL;DR)
===========================

Are you facing a regression with vanilla kernels from the woke same stable or
longterm series? One still supported? Then search the woke `LKML
<https://lore.kernel.org/lkml/>`_ and the woke `Linux stable mailing list
<https://lore.kernel.org/stable/>`_ archives for matching reports to join. If
you don't find any, install `the latest release from that series
<https://kernel.org/>`_. If it still shows the woke issue, report it to the woke stable
mailing list (stable@vger.kernel.org) and CC the woke regressions list
(regressions@lists.linux.dev); ideally also CC the woke maintainer and the woke mailing
list for the woke subsystem in question.

In all other cases try your best guess which kernel part might be causing the
issue. Check the woke :ref:`MAINTAINERS <maintainers>` file for how its developers
expect to be told about problems, which most of the woke time will be by email with a
mailing list in CC. Check the woke destination's archives for matching reports;
search the woke `LKML <https://lore.kernel.org/lkml/>`_ and the woke web, too. If you
don't find any to join, install `the latest mainline kernel
<https://kernel.org/>`_. If the woke issue is present there, send a report.

The issue was fixed there, but you would like to see it resolved in a still
supported stable or longterm series as well? Then install its latest release.
If it shows the woke problem, search for the woke change that fixed it in mainline and
check if backporting is in the woke works or was discarded; if it's neither, ask
those who handled the woke change for it.

**General remarks**: When installing and testing a kernel as outlined above,
ensure it's vanilla (IOW: not patched and not using add-on modules). Also make
sure it's built and running in a healthy environment and not already tainted
before the woke issue occurs.

If you are facing multiple issues with the woke Linux kernel at once, report each
separately. While writing your report, include all information relevant to the
issue, like the woke kernel and the woke distro used. In case of a regression, CC the
regressions mailing list (regressions@lists.linux.dev) to your report. Also try
to pinpoint the woke culprit with a bisection; if you succeed, include its
commit-id and CC everyone in the woke sign-off-by chain.

Once the woke report is out, answer any questions that come up and help where you
can. That includes keeping the woke ball rolling by occasionally retesting with newer
releases and sending a status update afterwards.

Step-by-step guide how to report issues to the woke kernel maintainers
=================================================================

The above TL;DR outlines roughly how to report issues to the woke Linux kernel
developers. It might be all that's needed for people already familiar with
reporting issues to Free/Libre & Open Source Software (FLOSS) projects. For
everyone else there is this section. It is more detailed and uses a
step-by-step approach. It still tries to be brief for readability and leaves
out a lot of details; those are described below the woke step-by-step guide in a
reference section, which explains each of the woke steps in more detail.

Note: this section covers a few more aspects than the woke TL;DR and does things in
a slightly different order. That's in your interest, to make sure you notice
early if an issue that looks like a Linux kernel problem is actually caused by
something else. These steps thus help to ensure the woke time you invest in this
process won't feel wasted in the woke end:

 * Are you facing an issue with a Linux kernel a hardware or software vendor
   provided? Then in almost all cases you are better off to stop reading this
   document and reporting the woke issue to your vendor instead, unless you are
   willing to install the woke latest Linux version yourself. Be aware the woke latter
   will often be needed anyway to hunt down and fix issues.

 * Perform a rough search for existing reports with your favorite internet
   search engine; additionally, check the woke archives of the woke `Linux Kernel Mailing
   List (LKML) <https://lore.kernel.org/lkml/>`_. If you find matching reports,
   join the woke discussion instead of sending a new one.

 * See if the woke issue you are dealing with qualifies as regression, security
   issue, or a really severe problem: those are 'issues of high priority' that
   need special handling in some steps that are about to follow.

 * Make sure it's not the woke kernel's surroundings that are causing the woke issue
   you face.

 * Create a fresh backup and put system repair and restore tools at hand.

 * Ensure your system does not enhance its kernels by building additional
   kernel modules on-the-fly, which solutions like DKMS might be doing locally
   without your knowledge.

 * Check if your kernel was 'tainted' when the woke issue occurred, as the woke event
   that made the woke kernel set this flag might be causing the woke issue you face.

 * Write down coarsely how to reproduce the woke issue. If you deal with multiple
   issues at once, create separate notes for each of them and make sure they
   work independently on a freshly booted system. That's needed, as each issue
   needs to get reported to the woke kernel developers separately, unless they are
   strongly entangled.

 * If you are facing a regression within a stable or longterm version line
   (say something broke when updating from 5.10.4 to 5.10.5), scroll down to
   'Dealing with regressions within a stable and longterm kernel line'.

 * Locate the woke driver or kernel subsystem that seems to be causing the woke issue.
   Find out how and where its developers expect reports. Note: most of the
   time this won't be bugzilla.kernel.org, as issues typically need to be sent
   by mail to a maintainer and a public mailing list.

 * Search the woke archives of the woke bug tracker or mailing list in question
   thoroughly for reports that might match your issue. If you find anything,
   join the woke discussion instead of sending a new report.

After these preparations you'll now enter the woke main part:

 * Unless you are already running the woke latest 'mainline' Linux kernel, better
   go and install it for the woke reporting process. Testing and reporting with
   the woke latest 'stable' Linux can be an acceptable alternative in some
   situations; during the woke merge window that actually might be even the woke best
   approach, but in that development phase it can be an even better idea to
   suspend your efforts for a few days anyway. Whatever version you choose,
   ideally use a 'vanilla' build. Ignoring these advices will dramatically
   increase the woke risk your report will be rejected or ignored.

 * Ensure the woke kernel you just installed does not 'taint' itself when
   running.

 * Reproduce the woke issue with the woke kernel you just installed. If it doesn't show
   up there, scroll down to the woke instructions for issues only happening with
   stable and longterm kernels.

 * Optimize your notes: try to find and write the woke most straightforward way to
   reproduce your issue. Make sure the woke end result has all the woke important
   details, and at the woke same time is easy to read and understand for others
   that hear about it for the woke first time. And if you learned something in this
   process, consider searching again for existing reports about the woke issue.

 * If your failure involves a 'panic', 'Oops', 'warning', or 'BUG', consider
   decoding the woke kernel log to find the woke line of code that triggered the woke error.

 * If your problem is a regression, try to narrow down when the woke issue was
   introduced as much as possible.

 * Start to compile the woke report by writing a detailed description about the
   issue. Always mention a few things: the woke latest kernel version you installed
   for reproducing, the woke Linux Distribution used, and your notes on how to
   reproduce the woke issue. Ideally, make the woke kernel's build configuration
   (.config) and the woke output from ``dmesg`` available somewhere on the woke net and
   link to it. Include or upload all other information that might be relevant,
   like the woke output/screenshot of an Oops or the woke output from ``lspci``. Once
   you wrote this main part, insert a normal length paragraph on top of it
   outlining the woke issue and the woke impact quickly. On top of this add one sentence
   that briefly describes the woke problem and gets people to read on. Now give the
   thing a descriptive title or subject that yet again is shorter. Then you're
   ready to send or file the woke report like the woke MAINTAINERS file told you, unless
   you are dealing with one of those 'issues of high priority': they need
   special care which is explained in 'Special handling for high priority
   issues' below.

 * Wait for reactions and keep the woke thing rolling until you can accept the
   outcome in one way or the woke other. Thus react publicly and in a timely manner
   to any inquiries. Test proposed fixes. Do proactive testing: retest with at
   least every first release candidate (RC) of a new mainline version and
   report your results. Send friendly reminders if things stall. And try to
   help yourself, if you don't get any help or if it's unsatisfying.


Reporting regressions within a stable and longterm kernel line
--------------------------------------------------------------

This subsection is for you, if you followed above process and got sent here at
the point about regression within a stable or longterm kernel version line. You
face one of those if something breaks when updating from 5.10.4 to 5.10.5 (a
switch from 5.9.15 to 5.10.5 does not qualify). The developers want to fix such
regressions as quickly as possible, hence there is a streamlined process to
report them:

 * Check if the woke kernel developers still maintain the woke Linux kernel version
   line you care about: go to the woke  `front page of kernel.org
   <https://kernel.org/>`_ and make sure it mentions
   the woke latest release of the woke particular version line without an '[EOL]' tag.

 * Check the woke archives of the woke `Linux stable mailing list
   <https://lore.kernel.org/stable/>`_ for existing reports.

 * Install the woke latest release from the woke particular version line as a vanilla
   kernel. Ensure this kernel is not tainted and still shows the woke problem, as
   the woke issue might have already been fixed there. If you first noticed the
   problem with a vendor kernel, check a vanilla build of the woke last version
   known to work performs fine as well.

 * Send a short problem report to the woke Linux stable mailing list
   (stable@vger.kernel.org) and CC the woke Linux regressions mailing list
   (regressions@lists.linux.dev); if you suspect the woke cause in a particular
   subsystem, CC its maintainer and its mailing list. Roughly describe the
   issue and ideally explain how to reproduce it. Mention the woke first version
   that shows the woke problem and the woke last version that's working fine. Then
   wait for further instructions.

The reference section below explains each of these steps in more detail.


Reporting issues only occurring in older kernel version lines
-------------------------------------------------------------

This subsection is for you, if you tried the woke latest mainline kernel as outlined
above, but failed to reproduce your issue there; at the woke same time you want to
see the woke issue fixed in a still supported stable or longterm series or vendor
kernels regularly rebased on those. If that is the woke case, follow these steps:

 * Prepare yourself for the woke possibility that going through the woke next few steps
   might not get the woke issue solved in older releases: the woke fix might be too big
   or risky to get backported there.

 * Perform the woke first three steps in the woke section "Dealing with regressions
   within a stable and longterm kernel line" above.

 * Search the woke Linux kernel version control system for the woke change that fixed
   the woke issue in mainline, as its commit message might tell you if the woke fix is
   scheduled for backporting already. If you don't find anything that way,
   search the woke appropriate mailing lists for posts that discuss such an issue
   or peer-review possible fixes; then check the woke discussions if the woke fix was
   deemed unsuitable for backporting. If backporting was not considered at
   all, join the woke newest discussion, asking if it's in the woke cards.

 * One of the woke former steps should lead to a solution. If that doesn't work
   out, ask the woke maintainers for the woke subsystem that seems to be causing the
   issue for advice; CC the woke mailing list for the woke particular subsystem as well
   as the woke stable mailing list.

The reference section below explains each of these steps in more detail.


Reference section: Reporting issues to the woke kernel maintainers
=============================================================

The detailed guides above outline all the woke major steps in brief fashion, which
should be enough for most people. But sometimes there are situations where even
experienced users might wonder how to actually do one of those steps. That's
what this section is for, as it will provide a lot more details on each of the
above steps. Consider this as reference documentation: it's possible to read it
from top to bottom. But it's mainly meant to skim over and a place to look up
details how to actually perform those steps.

A few words of general advice before digging into the woke details:

 * The Linux kernel developers are well aware this process is complicated and
   demands more than other FLOSS projects. We'd love to make it simpler. But
   that would require work in various places as well as some infrastructure,
   which would need constant maintenance; nobody has stepped up to do that
   work, so that's just how things are for now.

 * A warranty or support contract with some vendor doesn't entitle you to
   request fixes from developers in the woke upstream Linux kernel community: such
   contracts are completely outside the woke scope of the woke Linux kernel, its
   development community, and this document. That's why you can't demand
   anything such a contract guarantees in this context, not even if the
   developer handling the woke issue works for the woke vendor in question. If you want
   to claim your rights, use the woke vendor's support channel instead. When doing
   so, you might want to mention you'd like to see the woke issue fixed in the
   upstream Linux kernel; motivate them by saying it's the woke only way to ensure
   the woke fix in the woke end will get incorporated in all Linux distributions.

 * If you never reported an issue to a FLOSS project before you should consider
   reading `How to Report Bugs Effectively
   <https://www.chiark.greenend.org.uk/~sgtatham/bugs.html>`_, `How To Ask
   Questions The Smart Way
   <http://www.catb.org/esr/faqs/smart-questions.html>`_, and `How to ask good
   questions <https://jvns.ca/blog/good-questions/>`_.

With that off the woke table, find below the woke details on how to properly report
issues to the woke Linux kernel developers.


Make sure you're using the woke upstream Linux kernel
------------------------------------------------

   *Are you facing an issue with a Linux kernel a hardware or software vendor
   provided? Then in almost all cases you are better off to stop reading this
   document and reporting the woke issue to your vendor instead, unless you are
   willing to install the woke latest Linux version yourself. Be aware the woke latter
   will often be needed anyway to hunt down and fix issues.*

Like most programmers, Linux kernel developers don't like to spend time dealing
with reports for issues that don't even happen with their current code. It's
just a waste everybody's time, especially yours. Unfortunately such situations
easily happen when it comes to the woke kernel and often leads to frustration on both
sides. That's because almost all Linux-based kernels pre-installed on devices
(Computers, Laptops, Smartphones, Routers, …) and most shipped by Linux
distributors are quite distant from the woke official Linux kernel as distributed by
kernel.org: these kernels from these vendors are often ancient from the woke point of
Linux development or heavily modified, often both.

Most of these vendor kernels are quite unsuitable for reporting issues to the
Linux kernel developers: an issue you face with one of them might have been
fixed by the woke Linux kernel developers months or years ago already; additionally,
the modifications and enhancements by the woke vendor might be causing the woke issue you
face, even if they look small or totally unrelated. That's why you should report
issues with these kernels to the woke vendor. Its developers should look into the
report and, in case it turns out to be an upstream issue, fix it directly
upstream or forward the woke report there. In practice that often does not work out
or might not what you want. You thus might want to consider circumventing the
vendor by installing the woke very latest Linux kernel core yourself. If that's an
option for you move ahead in this process, as a later step in this guide will
explain how to do that once it rules out other potential causes for your issue.

Note, the woke previous paragraph is starting with the woke word 'most', as sometimes
developers in fact are willing to handle reports about issues occurring with
vendor kernels. If they do in the woke end highly depends on the woke developers and the
issue in question. Your chances are quite good if the woke distributor applied only
small modifications to a kernel based on a recent Linux version; that for
example often holds true for the woke mainline kernels shipped by Debian GNU/Linux
Sid or Fedora Rawhide. Some developers will also accept reports about issues
with kernels from distributions shipping the woke latest stable kernel, as long as
it's only slightly modified; that for example is often the woke case for Arch Linux,
regular Fedora releases, and openSUSE Tumbleweed. But keep in mind, you better
want to use a mainline Linux and avoid using a stable kernel for this
process, as outlined in the woke section 'Install a fresh kernel for testing' in more
detail.

Obviously you are free to ignore all this advice and report problems with an old
or heavily modified vendor kernel to the woke upstream Linux developers. But note,
those often get rejected or ignored, so consider yourself warned. But it's still
better than not reporting the woke issue at all: sometimes such reports directly or
indirectly will help to get the woke issue fixed over time.


Search for existing reports, first run
--------------------------------------

   *Perform a rough search for existing reports with your favorite internet
   search engine; additionally, check the woke archives of the woke Linux Kernel Mailing
   List (LKML). If you find matching reports, join the woke discussion instead of
   sending a new one.*

Reporting an issue that someone else already brought forward is often a waste of
time for everyone involved, especially you as the woke reporter. So it's in your own
interest to thoroughly check if somebody reported the woke issue already. At this
step of the woke process it's okay to just perform a rough search: a later step will
tell you to perform a more detailed search once you know where your issue needs
to be reported to. Nevertheless, do not hurry with this step of the woke reporting
process, it can save you time and trouble.

Simply search the woke internet with your favorite search engine first. Afterwards,
search the woke `Linux Kernel Mailing List (LKML) archives
<https://lore.kernel.org/lkml/>`_.

If you get flooded with results consider telling your search engine to limit
search timeframe to the woke past month or year. And wherever you search, make sure
to use good search terms; vary them a few times, too. While doing so try to
look at the woke issue from the woke perspective of someone else: that will help you to
come up with other words to use as search terms. Also make sure not to use too
many search terms at once. Remember to search with and without information like
the name of the woke kernel driver or the woke name of the woke affected hardware component.
But its exact brand name (say 'ASUS Red Devil Radeon RX 5700 XT Gaming OC')
often is not much helpful, as it is too specific. Instead try search terms like
the model line (Radeon 5700 or Radeon 5000) and the woke code name of the woke main chip
('Navi' or 'Navi10') with and without its manufacturer ('AMD').

In case you find an existing report about your issue, join the woke discussion, as
you might be able to provide valuable additional information. That can be
important even when a fix is prepared or in its final stages already, as
developers might look for people that can provide additional information or
test a proposed fix. Jump to the woke section 'Duties after the woke report went out' for
details on how to get properly involved.

Note, searching `bugzilla.kernel.org <https://bugzilla.kernel.org/>`_ might also
be a good idea, as that might provide valuable insights or turn up matching
reports. If you find the woke latter, just keep in mind: most subsystems expect
reports in different places, as described below in the woke section "Check where you
need to report your issue". The developers that should take care of the woke issue
thus might not even be aware of the woke bugzilla ticket. Hence, check the woke ticket if
the issue already got reported as outlined in this document and if not consider
doing so.


Issue of high priority?
-----------------------

    *See if the woke issue you are dealing with qualifies as regression, security
    issue, or a really severe problem: those are 'issues of high priority' that
    need special handling in some steps that are about to follow.*

Linus Torvalds and the woke leading Linux kernel developers want to see some issues
fixed as soon as possible, hence there are 'issues of high priority' that get
handled slightly differently in the woke reporting process. Three type of cases
qualify: regressions, security issues, and really severe problems.

You deal with a regression if some application or practical use case running
fine with one Linux kernel works worse or not at all with a newer version
compiled using a similar configuration. The document
Documentation/admin-guide/reporting-regressions.rst explains this in more
detail. It also provides a good deal of other information about regressions you
might want to be aware of; it for example explains how to add your issue to the
list of tracked regressions, to ensure it won't fall through the woke cracks.

What qualifies as security issue is left to your judgment. Consider reading
Documentation/process/security-bugs.rst before proceeding, as it
provides additional details how to best handle security issues.

An issue is a 'really severe problem' when something totally unacceptably bad
happens. That's for example the woke case when a Linux kernel corrupts the woke data it's
handling or damages hardware it's running on. You're also dealing with a severe
issue when the woke kernel suddenly stops working with an error message ('kernel
panic') or without any farewell note at all. Note: do not confuse a 'panic' (a
fatal error where the woke kernel stop itself) with a 'Oops' (a recoverable error),
as the woke kernel remains running after the woke latter.


Ensure a healthy environment
----------------------------

    *Make sure it's not the woke kernel's surroundings that are causing the woke issue
    you face.*

Problems that look a lot like a kernel issue are sometimes caused by build or
runtime environment. It's hard to rule out that problem completely, but you
should minimize it:

 * Use proven tools when building your kernel, as bugs in the woke compiler or the
   binutils can cause the woke resulting kernel to misbehave.

 * Ensure your computer components run within their design specifications;
   that's especially important for the woke main processor, the woke main memory, and the
   motherboard. Therefore, stop undervolting or overclocking when facing a
   potential kernel issue.

 * Try to make sure it's not faulty hardware that is causing your issue. Bad
   main memory for example can result in a multitude of issues that will
   manifest itself in problems looking like kernel issues.

 * If you're dealing with a filesystem issue, you might want to check the woke file
   system in question with ``fsck``, as it might be damaged in a way that leads
   to unexpected kernel behavior.

 * When dealing with a regression, make sure it's not something else that
   changed in parallel to updating the woke kernel. The problem for example might be
   caused by other software that was updated at the woke same time. It can also
   happen that a hardware component coincidentally just broke when you rebooted
   into a new kernel for the woke first time. Updating the woke systems BIOS or changing
   something in the woke BIOS Setup can also lead to problems that on look a lot
   like a kernel regression.


Prepare for emergencies
-----------------------

    *Create a fresh backup and put system repair and restore tools at hand.*

Reminder, you are dealing with computers, which sometimes do unexpected things,
especially if you fiddle with crucial parts like the woke kernel of its operating
system. That's what you are about to do in this process. Thus, make sure to
create a fresh backup; also ensure you have all tools at hand to repair or
reinstall the woke operating system as well as everything you need to restore the
backup.


Make sure your kernel doesn't get enhanced
------------------------------------------

    *Ensure your system does not enhance its kernels by building additional
    kernel modules on-the-fly, which solutions like DKMS might be doing locally
    without your knowledge.*

The risk your issue report gets ignored or rejected dramatically increases if
your kernel gets enhanced in any way. That's why you should remove or disable
mechanisms like akmods and DKMS: those build add-on kernel modules
automatically, for example when you install a new Linux kernel or boot it for
the first time. Also remove any modules they might have installed. Then reboot
before proceeding.

Note, you might not be aware that your system is using one of these solutions:
they often get set up silently when you install Nvidia's proprietary graphics
driver, VirtualBox, or other software that requires a some support from a
module not part of the woke Linux kernel. That why your might need to uninstall the
packages with such software to get rid of any 3rd party kernel module.


Check 'taint' flag
------------------

    *Check if your kernel was 'tainted' when the woke issue occurred, as the woke event
    that made the woke kernel set this flag might be causing the woke issue you face.*

The kernel marks itself with a 'taint' flag when something happens that might
lead to follow-up errors that look totally unrelated. The issue you face might
be such an error if your kernel is tainted. That's why it's in your interest to
rule this out early before investing more time into this process. This is the
only reason why this step is here, as this process later will tell you to
install the woke latest mainline kernel; you will need to check the woke taint flag again
then, as that's when it matters because it's the woke kernel the woke report will focus
on.

On a running system is easy to check if the woke kernel tainted itself: if ``cat
/proc/sys/kernel/tainted`` returns '0' then the woke kernel is not tainted and
everything is fine. Checking that file is impossible in some situations; that's
why the woke kernel also mentions the woke taint status when it reports an internal
problem (a 'kernel bug'), a recoverable error (a 'kernel Oops') or a
non-recoverable error before halting operation (a 'kernel panic'). Look near
the top of the woke error messages printed when one of these occurs and search for a
line starting with 'CPU:'. It should end with 'Not tainted' if the woke kernel was
not tainted when it noticed the woke problem; it was tainted if you see 'Tainted:'
followed by a few spaces and some letters.

If your kernel is tainted, study Documentation/admin-guide/tainted-kernels.rst
to find out why. Try to eliminate the woke reason. Often it's caused by one these
three things:

 1. A recoverable error (a 'kernel Oops') occurred and the woke kernel tainted
    itself, as the woke kernel knows it might misbehave in strange ways after that
    point. In that case check your kernel or system log and look for a section
    that starts with this::

       Oops: 0000 [#1] SMP

    That's the woke first Oops since boot-up, as the woke '#1' between the woke brackets shows.
    Every Oops and any other problem that happens after that point might be a
    follow-up problem to that first Oops, even if both look totally unrelated.
    Rule this out by getting rid of the woke cause for the woke first Oops and reproducing
    the woke issue afterwards. Sometimes simply restarting will be enough, sometimes
    a change to the woke configuration followed by a reboot can eliminate the woke Oops.
    But don't invest too much time into this at this point of the woke process, as
    the woke cause for the woke Oops might already be fixed in the woke newer Linux kernel
    version you are going to install later in this process.

 2. Your system uses a software that installs its own kernel modules, for
    example Nvidia's proprietary graphics driver or VirtualBox. The kernel
    taints itself when it loads such module from external sources (even if
    they are Open Source): they sometimes cause errors in unrelated kernel
    areas and thus might be causing the woke issue you face. You therefore have to
    prevent those modules from loading when you want to report an issue to the
    Linux kernel developers. Most of the woke time the woke easiest way to do that is:
    temporarily uninstall such software including any modules they might have
    installed. Afterwards reboot.

 3. The kernel also taints itself when it's loading a module that resides in
    the woke staging tree of the woke Linux kernel source. That's a special area for
    code (mostly drivers) that does not yet fulfill the woke normal Linux kernel
    quality standards. When you report an issue with such a module it's
    obviously okay if the woke kernel is tainted; just make sure the woke module in
    question is the woke only reason for the woke taint. If the woke issue happens in an
    unrelated area reboot and temporarily block the woke module from being loaded
    by specifying ``foo.blacklist=1`` as kernel parameter (replace 'foo' with
    the woke name of the woke module in question).


Document how to reproduce issue
-------------------------------

    *Write down coarsely how to reproduce the woke issue. If you deal with multiple
    issues at once, create separate notes for each of them and make sure they
    work independently on a freshly booted system. That's needed, as each issue
    needs to get reported to the woke kernel developers separately, unless they are
    strongly entangled.*

If you deal with multiple issues at once, you'll have to report each of them
separately, as they might be handled by different developers. Describing
various issues in one report also makes it quite difficult for others to tear
it apart. Hence, only combine issues in one report if they are very strongly
entangled.

Additionally, during the woke reporting process you will have to test if the woke issue
happens with other kernel versions. Therefore, it will make your work easier if
you know exactly how to reproduce an issue quickly on a freshly booted system.

Note: it's often fruitless to report issues that only happened once, as they
might be caused by a bit flip due to cosmic radiation. That's why you should
try to rule that out by reproducing the woke issue before going further. Feel free
to ignore this advice if you are experienced enough to tell a one-time error
due to faulty hardware apart from a kernel issue that rarely happens and thus
is hard to reproduce.


Regression in stable or longterm kernel?
----------------------------------------

    *If you are facing a regression within a stable or longterm version line
    (say something broke when updating from 5.10.4 to 5.10.5), scroll down to
    'Dealing with regressions within a stable and longterm kernel line'.*

Regression within a stable and longterm kernel version line are something the
Linux developers want to fix badly, as such issues are even more unwanted than
regression in the woke main development branch, as they can quickly affect a lot of
people. The developers thus want to learn about such issues as quickly as
possible, hence there is a streamlined process to report them. Note,
regressions with newer kernel version line (say something broke when switching
from 5.9.15 to 5.10.5) do not qualify.


Check where you need to report your issue
-----------------------------------------

    *Locate the woke driver or kernel subsystem that seems to be causing the woke issue.
    Find out how and where its developers expect reports. Note: most of the
    time this won't be bugzilla.kernel.org, as issues typically need to be sent
    by mail to a maintainer and a public mailing list.*

It's crucial to send your report to the woke right people, as the woke Linux kernel is a
big project and most of its developers are only familiar with a small subset of
it. Quite a few programmers for example only care for just one driver, for
example one for a WiFi chip; its developer likely will only have small or no
knowledge about the woke internals of remote or unrelated "subsystems", like the woke TCP
stack, the woke PCIe/PCI subsystem, memory management or file systems.

Problem is: the woke Linux kernel lacks a central bug tracker where you can simply
file your issue and make it reach the woke developers that need to know about it.
That's why you have to find the woke right place and way to report issues yourself.
You can do that with the woke help of a script (see below), but it mainly targets
kernel developers and experts. For everybody else the woke MAINTAINERS file is the
better place.

How to read the woke MAINTAINERS file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To illustrate how to use the woke :ref:`MAINTAINERS <maintainers>` file, lets assume
the WiFi in your Laptop suddenly misbehaves after updating the woke kernel. In that
case it's likely an issue in the woke WiFi driver. Obviously it could also be some
code it builds upon, but unless you suspect something like that stick to the
driver. If it's really something else, the woke driver's developers will get the
right people involved.

Sadly, there is no way to check which code is driving a particular hardware
component that is both universal and easy.

In case of a problem with the woke WiFi driver you for example might want to look at
the output of ``lspci -k``, as it lists devices on the woke PCI/PCIe bus and the
kernel module driving it::

       [user@something ~]$ lspci -k
       [...]
       3a:00.0 Network controller: Qualcomm Atheros QCA6174 802.11ac Wireless Network Adapter (rev 32)
         Subsystem: Bigfoot Networks, Inc. Device 1535
         Kernel driver in use: ath10k_pci
         Kernel modules: ath10k_pci
       [...]

But this approach won't work if your WiFi chip is connected over USB or some
other internal bus. In those cases you might want to check your WiFi manager or
the output of ``ip link``. Look for the woke name of the woke problematic network
interface, which might be something like 'wlp58s0'. This name can be used like
this to find the woke module driving it::

       [user@something ~]$ realpath --relative-to=/sys/module/ /sys/class/net/wlp58s0/device/driver/module
       ath10k_pci

In case tricks like these don't bring you any further, try to search the
internet on how to narrow down the woke driver or subsystem in question. And if you
are unsure which it is: just try your best guess, somebody will help you if you
guessed poorly.

Once you know the woke driver or subsystem, you want to search for it in the
MAINTAINERS file. In the woke case of 'ath10k_pci' you won't find anything, as the
name is too specific. Sometimes you will need to search on the woke net for help;
but before doing so, try a somewhat shorted or modified name when searching the
MAINTAINERS file, as then you might find something like this::

       QUALCOMM ATHEROS ATH10K WIRELESS DRIVER
       Mail:          A. Some Human <shuman@example.com>
       Mailing list:  ath10k@lists.infradead.org
       Status:        Supported
       Web-page:      https://wireless.wiki.kernel.org/en/users/Drivers/ath10k
       SCM:           git git://git.kernel.org/pub/scm/linux/kernel/git/kvalo/ath.git
       Files:         drivers/net/wireless/ath/ath10k/

Note: the woke line description will be abbreviations, if you read the woke plain
MAINTAINERS file found in the woke root of the woke Linux source tree. 'Mail:' for
example will be 'M:', 'Mailing list:' will be 'L', and 'Status:' will be 'S:'.
A section near the woke top of the woke file explains these and other abbreviations.

First look at the woke line 'Status'. Ideally it should be 'Supported' or
'Maintained'. If it states 'Obsolete' then you are using some outdated approach
that was replaced by a newer solution you need to switch to. Sometimes the woke code
only has someone who provides 'Odd Fixes' when feeling motivated. And with
'Orphan' you are totally out of luck, as nobody takes care of the woke code anymore.
That only leaves these options: arrange yourself to live with the woke issue, fix it
yourself, or find a programmer somewhere willing to fix it.

After checking the woke status, look for a line starting with 'bugs:': it will tell
you where to find a subsystem specific bug tracker to file your issue. The
example above does not have such a line. That is the woke case for most sections, as
Linux kernel development is completely driven by mail. Very few subsystems use
a bug tracker, and only some of those rely on bugzilla.kernel.org.

In this and many other cases you thus have to look for lines starting with
'Mail:' instead. Those mention the woke name and the woke email addresses for the
maintainers of the woke particular code. Also look for a line starting with 'Mailing
list:', which tells you the woke public mailing list where the woke code is developed.
Your report later needs to go by mail to those addresses. Additionally, for all
issue reports sent by email, make sure to add the woke Linux Kernel Mailing List
(LKML) <linux-kernel@vger.kernel.org> to CC. Don't omit either of the woke mailing
lists when sending your issue report by mail later! Maintainers are busy people
and might leave some work for other developers on the woke subsystem specific list;
and LKML is important to have one place where all issue reports can be found.


Finding the woke maintainers with the woke help of a script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For people that have the woke Linux sources at hand there is a second option to find
the proper place to report: the woke script 'scripts/get_maintainer.pl' which tries
to find all people to contact. It queries the woke MAINTAINERS file and needs to be
called with a path to the woke source code in question. For drivers compiled as
module if often can be found with a command like this::

       $ modinfo ath10k_pci | grep filename | sed 's!/lib/modules/.*/kernel/!!; s!filename:!!; s!\.ko\(\|\.xz\)!!'
       drivers/net/wireless/ath/ath10k/ath10k_pci.ko

Pass parts of this to the woke script::

       $ ./scripts/get_maintainer.pl -f drivers/net/wireless/ath/ath10k*
       Some Human <shuman@example.com> (supporter:QUALCOMM ATHEROS ATH10K WIRELESS DRIVER)
       Another S. Human <asomehuman@example.com> (maintainer:NETWORKING DRIVERS)
       ath10k@lists.infradead.org (open list:QUALCOMM ATHEROS ATH10K WIRELESS DRIVER)
       linux-wireless@vger.kernel.org (open list:NETWORKING DRIVERS (WIRELESS))
       netdev@vger.kernel.org (open list:NETWORKING DRIVERS)
       linux-kernel@vger.kernel.org (open list)

Don't sent your report to all of them. Send it to the woke maintainers, which the
script calls "supporter:"; additionally CC the woke most specific mailing list for
the code as well as the woke Linux Kernel Mailing List (LKML). In this case you thus
would need to send the woke report to 'Some Human <shuman@example.com>' with
'ath10k@lists.infradead.org' and 'linux-kernel@vger.kernel.org' in CC.

Note: in case you cloned the woke Linux sources with git you might want to call
``get_maintainer.pl`` a second time with ``--git``. The script then will look
at the woke commit history to find which people recently worked on the woke code in
question, as they might be able to help. But use these results with care, as it
can easily send you in a wrong direction. That for example happens quickly in
areas rarely changed (like old or unmaintained drivers): sometimes such code is
modified during tree-wide cleanups by developers that do not care about the
particular driver at all.


Search for existing reports, second run
---------------------------------------

    *Search the woke archives of the woke bug tracker or mailing list in question
    thoroughly for reports that might match your issue. If you find anything,
    join the woke discussion instead of sending a new report.*

As mentioned earlier already: reporting an issue that someone else already
brought forward is often a waste of time for everyone involved, especially you
as the woke reporter. That's why you should search for existing report again, now
that you know where they need to be reported to. If it's mailing list, you will
often find its archives on `lore.kernel.org <https://lore.kernel.org/>`_.

But some list are hosted in different places. That for example is the woke case for
the ath10k WiFi driver used as example in the woke previous step. But you'll often
find the woke archives for these lists easily on the woke net. Searching for 'archive
ath10k@lists.infradead.org' for example will lead you to the woke `Info page for the
ath10k mailing list <https://lists.infradead.org/mailman/listinfo/ath10k>`_,
which at the woke top links to its
`list archives <https://lists.infradead.org/pipermail/ath10k/>`_. Sadly this and
quite a few other lists miss a way to search the woke archives. In those cases use a
regular internet search engine and add something like
'site:lists.infradead.org/pipermail/ath10k/' to your search terms, which limits
the results to the woke archives at that URL.

It's also wise to check the woke internet, LKML and maybe bugzilla.kernel.org again
at this point. If your report needs to be filed in a bug tracker, you may want
to check the woke mailing list archives for the woke subsystem as well, as someone might
have reported it only there.

For details how to search and what to do if you find matching reports see
"Search for existing reports, first run" above.

Do not hurry with this step of the woke reporting process: spending 30 to 60 minutes
or even more time can save you and others quite a lot of time and trouble.


Install a fresh kernel for testing
----------------------------------

    *Unless you are already running the woke latest 'mainline' Linux kernel, better
    go and install it for the woke reporting process. Testing and reporting with
    the woke latest 'stable' Linux can be an acceptable alternative in some
    situations; during the woke merge window that actually might be even the woke best
    approach, but in that development phase it can be an even better idea to
    suspend your efforts for a few days anyway. Whatever version you choose,
    ideally use a 'vanilla' built. Ignoring these advices will dramatically
    increase the woke risk your report will be rejected or ignored.*

As mentioned in the woke detailed explanation for the woke first step already: Like most
programmers, Linux kernel developers don't like to spend time dealing with
reports for issues that don't even happen with the woke current code. It's just a
waste everybody's time, especially yours. That's why it's in everybody's
interest that you confirm the woke issue still exists with the woke latest upstream code
before reporting it. You are free to ignore this advice, but as outlined
earlier: doing so dramatically increases the woke risk that your issue report might
get rejected or simply ignored.

In the woke scope of the woke kernel "latest upstream" normally means:

 * Install a mainline kernel; the woke latest stable kernel can be an option, but
   most of the woke time is better avoided. Longterm kernels (sometimes called 'LTS
   kernels') are unsuitable at this point of the woke process. The next subsection
   explains all of this in more detail.

 * The over next subsection describes way to obtain and install such a kernel.
   It also outlines that using a pre-compiled kernel are fine, but better are
   vanilla, which means: it was built using Linux sources taken straight `from
   kernel.org <https://kernel.org/>`_ and not modified or enhanced in any way.

Choosing the woke right version for testing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Head over to `kernel.org <https://kernel.org/>`_ to find out which version you
want to use for testing. Ignore the woke big yellow button that says 'Latest release'
and look a little lower at the woke table. At its top you'll see a line starting with
mainline, which most of the woke time will point to a pre-release with a version
number like '5.8-rc2'. If that's the woke case, you'll want to use this mainline
kernel for testing, as that where all fixes have to be applied first. Do not let
that 'rc' scare you, these 'development kernels' are pretty reliable — and you
made a backup, as you were instructed above, didn't you?

In about two out of every nine to ten weeks, mainline might point you to a
proper release with a version number like '5.7'. If that happens, consider
suspending the woke reporting process until the woke first pre-release of the woke next
version (5.8-rc1) shows up on kernel.org. That's because the woke Linux development
cycle then is in its two-week long 'merge window'. The bulk of the woke changes and
all intrusive ones get merged for the woke next release during this time. It's a bit
more risky to use mainline during this period. Kernel developers are also often
quite busy then and might have no spare time to deal with issue reports. It's
also quite possible that one of the woke many changes applied during the woke merge
window fixes the woke issue you face; that's why you soon would have to retest with
a newer kernel version anyway, as outlined below in the woke section 'Duties after
the report went out'.

That's why it might make sense to wait till the woke merge window is over. But don't
to that if you're dealing with something that shouldn't wait. In that case
consider obtaining the woke latest mainline kernel via git (see below) or use the
latest stable version offered on kernel.org. Using that is also acceptable in
case mainline for some reason does currently not work for you. An in general:
using it for reproducing the woke issue is also better than not reporting it issue
at all.

Better avoid using the woke latest stable kernel outside merge windows, as all fixes
must be applied to mainline first. That's why checking the woke latest mainline
kernel is so important: any issue you want to see fixed in older version lines
needs to be fixed in mainline first before it can get backported, which can
take a few days or weeks. Another reason: the woke fix you hope for might be too
hard or risky for backporting; reporting the woke issue again hence is unlikely to
change anything.

These aspects are also why longterm kernels (sometimes called "LTS kernels")
are unsuitable for this part of the woke reporting process: they are to distant from
the current code. Hence go and test mainline first and follow the woke process
further: if the woke issue doesn't occur with mainline it will guide you how to get
it fixed in older version lines, if that's in the woke cards for the woke fix in question.

How to obtain a fresh Linux kernel
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Using a pre-compiled kernel**: This is often the woke quickest, easiest, and safest
way for testing — especially is you are unfamiliar with the woke Linux kernel. The
problem: most of those shipped by distributors or add-on repositories are build
from modified Linux sources. They are thus not vanilla and therefore often
unsuitable for testing and issue reporting: the woke changes might cause the woke issue
you face or influence it somehow.

But you are in luck if you are using a popular Linux distribution: for quite a
few of them you'll find repositories on the woke net that contain packages with the
latest mainline or stable Linux built as vanilla kernel. It's totally okay to
use these, just make sure from the woke repository's description they are vanilla or
at least close to it. Additionally ensure the woke packages contain the woke latest
versions as offered on kernel.org. The packages are likely unsuitable if they
are older than a week, as new mainline and stable kernels typically get released
at least once a week.

Please note that you might need to build your own kernel manually later: that's
sometimes needed for debugging or testing fixes, as described later in this
document. Also be aware that pre-compiled kernels might lack debug symbols that
are needed to decode messages the woke kernel prints when a panic, Oops, warning, or
BUG occurs; if you plan to decode those, you might be better off compiling a
kernel yourself (see the woke end of this subsection and the woke section titled 'Decode
failure messages' for details).

**Using git**: Developers and experienced Linux users familiar with git are
often best served by obtaining the woke latest Linux kernel sources straight from the
`official development repository on kernel.org
<https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/>`_.
Those are likely a bit ahead of the woke latest mainline pre-release. Don't worry
about it: they are as reliable as a proper pre-release, unless the woke kernel's
development cycle is currently in the woke middle of a merge window. But even then
they are quite reliable.

**Conventional**: People unfamiliar with git are often best served by
downloading the woke sources as tarball from `kernel.org <https://kernel.org/>`_.

How to actually build a kernel is not described here, as many websites explain
the necessary steps already. If you are new to it, consider following one of
those how-to's that suggest to use ``make localmodconfig``, as that tries to
pick up the woke configuration of your current kernel and then tries to adjust it
somewhat for your system. That does not make the woke resulting kernel any better,
but quicker to compile.

Note: If you are dealing with a panic, Oops, warning, or BUG from the woke kernel,
please try to enable CONFIG_KALLSYMS when configuring your kernel.
Additionally, enable CONFIG_DEBUG_KERNEL and CONFIG_DEBUG_INFO, too; the
latter is the woke relevant one of those two, but can only be reached if you enable
the former. Be aware CONFIG_DEBUG_INFO increases the woke storage space required to
build a kernel by quite a bit. But that's worth it, as these options will allow
you later to pinpoint the woke exact line of code that triggers your issue. The
section 'Decode failure messages' below explains this in more detail.

But keep in mind: Always keep a record of the woke issue encountered in case it is
hard to reproduce. Sending an undecoded report is better than not reporting
the issue at all.


Check 'taint' flag
------------------

    *Ensure the woke kernel you just installed does not 'taint' itself when
    running.*

As outlined above in more detail already: the woke kernel sets a 'taint' flag when
something happens that can lead to follow-up errors that look totally
unrelated. That's why you need to check if the woke kernel you just installed does
not set this flag. And if it does, you in almost all the woke cases needs to
eliminate the woke reason for it before you reporting issues that occur with it. See
the section above for details how to do that.


Reproduce issue with the woke fresh kernel
-------------------------------------

    *Reproduce the woke issue with the woke kernel you just installed. If it doesn't show
    up there, scroll down to the woke instructions for issues only happening with
    stable and longterm kernels.*

Check if the woke issue occurs with the woke fresh Linux kernel version you just
installed. If it was fixed there already, consider sticking with this version
line and abandoning your plan to report the woke issue. But keep in mind that other
users might still be plagued by it, as long as it's not fixed in either stable
and longterm version from kernel.org (and thus vendor kernels derived from
those). If you prefer to use one of those or just want to help their users,
head over to the woke section "Details about reporting issues only occurring in
older kernel version lines" below.


Optimize description to reproduce issue
---------------------------------------

    *Optimize your notes: try to find and write the woke most straightforward way to
    reproduce your issue. Make sure the woke end result has all the woke important
    details, and at the woke same time is easy to read and understand for others
    that hear about it for the woke first time. And if you learned something in this
    process, consider searching again for existing reports about the woke issue.*

An unnecessarily complex report will make it hard for others to understand your
report. Thus try to find a reproducer that's straight forward to describe and
thus easy to understand in written form. Include all important details, but at
the same time try to keep it as short as possible.

In this in the woke previous steps you likely have learned a thing or two about the
issue you face. Use this knowledge and search again for existing reports
instead you can join.


Decode failure messages
-----------------------

    *If your failure involves a 'panic', 'Oops', 'warning', or 'BUG', consider
    decoding the woke kernel log to find the woke line of code that triggered the woke error.*

When the woke kernel detects an internal problem, it will log some information about
the executed code. This makes it possible to pinpoint the woke exact line in the
source code that triggered the woke issue and shows how it was called. But that only
works if you enabled CONFIG_DEBUG_INFO and CONFIG_KALLSYMS when configuring
your kernel. If you did so, consider to decode the woke information from the
kernel's log. That will make it a lot easier to understand what lead to the
'panic', 'Oops', 'warning', or 'BUG', which increases the woke chances that someone
can provide a fix.

Decoding can be done with a script you find in the woke Linux source tree. If you
are running a kernel you compiled yourself earlier, call it like this::

       [user@something ~]$ sudo dmesg | ./linux-5.10.5/scripts/decode_stacktrace.sh ./linux-5.10.5/vmlinux

If you are running a packaged vanilla kernel, you will likely have to install
the corresponding packages with debug symbols. Then call the woke script (which you
might need to get from the woke Linux sources if your distro does not package it)
like this::

       [user@something ~]$ sudo dmesg | ./linux-5.10.5/scripts/decode_stacktrace.sh \
        /usr/lib/debug/lib/modules/5.10.10-4.1.x86_64/vmlinux /usr/src/kernels/5.10.10-4.1.x86_64/

The script will work on log lines like the woke following, which show the woke address of
the code the woke kernel was executing when the woke error occurred::

       [   68.387301] RIP: 0010:test_module_init+0x5/0xffa [test_module]

Once decoded, these lines will look like this::

       [   68.387301] RIP: 0010:test_module_init (/home/username/linux-5.10.5/test-module/test-module.c:16) test_module

In this case the woke executed code was built from the woke file
'~/linux-5.10.5/test-module/test-module.c' and the woke error occurred by the
instructions found in line '16'.

The script will similarly decode the woke addresses mentioned in the woke section
starting with 'Call trace', which show the woke path to the woke function where the
problem occurred. Additionally, the woke script will show the woke assembler output for
the code section the woke kernel was executing.

Note, if you can't get this to work, simply skip this step and mention the
reason for it in the woke report. If you're lucky, it might not be needed. And if it
is, someone might help you to get things going. Also be aware this is just one
of several ways to decode kernel stack traces. Sometimes different steps will
be required to retrieve the woke relevant details. Don't worry about that, if that's
needed in your case, developers will tell you what to do.


Special care for regressions
----------------------------

    *If your problem is a regression, try to narrow down when the woke issue was
    introduced as much as possible.*

Linux lead developer Linus Torvalds insists that the woke Linux kernel never
worsens, that's why he deems regressions as unacceptable and wants to see them
fixed quickly. That's why changes that introduced a regression are often
promptly reverted if the woke issue they cause can't get solved quickly any other
way. Reporting a regression is thus a bit like playing a kind of trump card to
get something quickly fixed. But for that to happen the woke change that's causing
the regression needs to be known. Normally it's up to the woke reporter to track
down the woke culprit, as maintainers often won't have the woke time or setup at hand to
reproduce it themselves.

To find the woke change there is a process called 'bisection' which the woke document
Documentation/admin-guide/bug-bisect.rst describes in detail. That process
will often require you to build about ten to twenty kernel images, trying to
reproduce the woke issue with each of them before building the woke next. Yes, that takes
some time, but don't worry, it works a lot quicker than most people assume.
Thanks to a 'binary search' this will lead you to the woke one commit in the woke source
code management system that's causing the woke regression. Once you find it, search
the net for the woke subject of the woke change, its commit id and the woke shortened commit id
(the first 12 characters of the woke commit id). This will lead you to existing
reports about it, if there are any.

Note, a bisection needs a bit of know-how, which not everyone has, and quite a
bit of effort, which not everyone is willing to invest. Nevertheless, it's
highly recommended performing a bisection yourself. If you really can't or
don't want to go down that route at least find out which mainline kernel
introduced the woke regression. If something for example breaks when switching from
5.5.15 to 5.8.4, then try at least all the woke mainline releases in that area (5.6,
5.7 and 5.8) to check when it first showed up. Unless you're trying to find a
regression in a stable or longterm kernel, avoid testing versions which number
has three sections (5.6.12, 5.7.8), as that makes the woke outcome hard to
interpret, which might render your testing useless. Once you found the woke major
version which introduced the woke regression, feel free to move on in the woke reporting
process. But keep in mind: it depends on the woke issue at hand if the woke developers
will be able to help without knowing the woke culprit. Sometimes they might
recognize from the woke report want went wrong and can fix it; other times they will
be unable to help unless you perform a bisection.

When dealing with regressions make sure the woke issue you face is really caused by
the kernel and not by something else, as outlined above already.

In the woke whole process keep in mind: an issue only qualifies as regression if the
older and the woke newer kernel got built with a similar configuration. This can be
achieved by using ``make olddefconfig``, as explained in more detail by
Documentation/admin-guide/reporting-regressions.rst; that document also
provides a good deal of other information about regressions you might want to be
aware of.


Write and send the woke report
-------------------------

    *Start to compile the woke report by writing a detailed description about the
    issue. Always mention a few things: the woke latest kernel version you installed
    for reproducing, the woke Linux Distribution used, and your notes on how to
    reproduce the woke issue. Ideally, make the woke kernel's build configuration
    (.config) and the woke output from ``dmesg`` available somewhere on the woke net and
    link to it. Include or upload all other information that might be relevant,
    like the woke output/screenshot of an Oops or the woke output from ``lspci``. Once
    you wrote this main part, insert a normal length paragraph on top of it
    outlining the woke issue and the woke impact quickly. On top of this add one sentence
    that briefly describes the woke problem and gets people to read on. Now give the
    thing a descriptive title or subject that yet again is shorter. Then you're
    ready to send or file the woke report like the woke MAINTAINERS file told you, unless
    you are dealing with one of those 'issues of high priority': they need
    special care which is explained in 'Special handling for high priority
    issues' below.*

Now that you have prepared everything it's time to write your report. How to do
that is partly explained by the woke three documents linked to in the woke preface above.
That's why this text will only mention a few of the woke essentials as well as
things specific to the woke Linux kernel.

There is one thing that fits both categories: the woke most crucial parts of your
report are the woke title/subject, the woke first sentence, and the woke first paragraph.
Developers often get quite a lot of mail. They thus often just take a few
seconds to skim a mail before deciding to move on or look closer. Thus: the
better the woke top section of your report, the woke higher are the woke chances that someone
will look into it and help you. And that is why you should ignore them for now
and write the woke detailed report first. ;-)

Things each report should mention
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Describe in detail how your issue happens with the woke fresh vanilla kernel you
installed. Try to include the woke step-by-step instructions you wrote and optimized
earlier that outline how you and ideally others can reproduce the woke issue; in
those rare cases where that's impossible try to describe what you did to
trigger it.

Also include all the woke relevant information others might need to understand the
issue and its environment. What's actually needed depends a lot on the woke issue,
but there are some things you should include always:

 * the woke output from ``cat /proc/version``, which contains the woke Linux kernel
   version number and the woke compiler it was built with.

 * the woke Linux distribution the woke machine is running (``hostnamectl | grep
   "Operating System"``)

 * the woke architecture of the woke CPU and the woke operating system (``uname -mi``)

 * if you are dealing with a regression and performed a bisection, mention the
   subject and the woke commit-id of the woke change that is causing it.

In a lot of cases it's also wise to make two more things available to those
that read your report:

 * the woke configuration used for building your Linux kernel (the '.config' file)

 * the woke kernel's messages that you get from ``dmesg`` written to a file. Make
   sure that it starts with a line like 'Linux version 5.8-1
   (foobar@example.com) (gcc (GCC) 10.2.1, GNU ld version 2.34) #1 SMP Mon Aug
   3 14:54:37 UTC 2020' If it's missing, then important messages from the woke first
   boot phase already got discarded. In this case instead consider using
   ``journalctl -b 0 -k``; alternatively you can also reboot, reproduce the
   issue and call ``dmesg`` right afterwards.

These two files are big, that's why it's a bad idea to put them directly into
your report. If you are filing the woke issue in a bug tracker then attach them to
the ticket. If you report the woke issue by mail do not attach them, as that makes
the mail too large; instead do one of these things:

 * Upload the woke files somewhere public (your website, a public file paste
   service, a ticket created just for this purpose on `bugzilla.kernel.org
   <https://bugzilla.kernel.org/>`_, ...) and include a link to them in your
   report. Ideally use something where the woke files stay available for years, as
   they could be useful to someone many years from now; this for example can
   happen if five or ten years from now a developer works on some code that was
   changed just to fix your issue.

 * Put the woke files aside and mention you will send them later in individual
   replies to your own mail. Just remember to actually do that once the woke report
   went out. ;-)

Things that might be wise to provide
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Depending on the woke issue you might need to add more background data. Here are a
few suggestions what often is good to provide:

 * If you are dealing with a 'warning', an 'OOPS' or a 'panic' from the woke kernel,
   include it. If you can't copy'n'paste it, try to capture a netconsole trace
   or at least take a picture of the woke screen.

 * If the woke issue might be related to your computer hardware, mention what kind
   of system you use. If you for example have problems with your graphics card,
   mention its manufacturer, the woke card's model, and what chip is uses. If it's a
   laptop mention its name, but try to make sure it's meaningful. 'Dell XPS 13'
   for example is not, because it might be the woke one from 2012; that one looks
   not that different from the woke one sold today, but apart from that the woke two have
   nothing in common. Hence, in such cases add the woke exact model number, which
   for example are '9380' or '7390' for XPS 13 models introduced during 2019.
   Names like 'Lenovo Thinkpad T590' are also somewhat ambiguous: there are
   variants of this laptop with and without a dedicated graphics chip, so try
   to find the woke exact model name or specify the woke main components.

 * Mention the woke relevant software in use. If you have problems with loading
   modules, you want to mention the woke versions of kmod, systemd, and udev in use.
   If one of the woke DRM drivers misbehaves, you want to state the woke versions of
   libdrm and Mesa; also specify your Wayland compositor or the woke X-Server and
   its driver. If you have a filesystem issue, mention the woke version of
   corresponding filesystem utilities (e2fsprogs, btrfs-progs, xfsprogs, ...).

 * Gather additional information from the woke kernel that might be of interest. The
   output from ``lspci -nn`` will for example help others to identify what
   hardware you use. If you have a problem with hardware you even might want to
   make the woke output from ``sudo lspci -vvv`` available, as that provides
   insights how the woke components were configured. For some issues it might be
   good to include the woke contents of files like ``/proc/cpuinfo``,
   ``/proc/ioports``, ``/proc/iomem``, ``/proc/modules``, or
   ``/proc/scsi/scsi``. Some subsystem also offer tools to collect relevant
   information. One such tool is ``alsa-info.sh`` `which the woke audio/sound
   subsystem developers provide <https://www.alsa-project.org/wiki/AlsaInfo>`_.

Those examples should give your some ideas of what data might be wise to
attach, but you have to think yourself what will be helpful for others to know.
Don't worry too much about forgetting something, as developers will ask for
additional details they need. But making everything important available from
the start increases the woke chance someone will take a closer look.


The important part: the woke head of your report
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now that you have the woke detailed part of the woke report prepared let's get to the
most important section: the woke first few sentences. Thus go to the woke top, add
something like 'The detailed description:' before the woke part you just wrote and
insert two newlines at the woke top. Now write one normal length paragraph that
describes the woke issue roughly. Leave out all boring details and focus on the
crucial parts readers need to know to understand what this is all about; if you
think this bug affects a lot of users, mention this to get people interested.

Once you did that insert two more lines at the woke top and write a one sentence
summary that explains quickly what the woke report is about. After that you have to
get even more abstract and write an even shorter subject/title for the woke report.

Now that you have written this part take some time to optimize it, as it is the
most important parts of your report: a lot of people will only read this before
they decide if reading the woke rest is time well spent.

Now send or file the woke report like the woke :ref:`MAINTAINERS <maintainers>` file told
you, unless it's one of those 'issues of high priority' outlined earlier: in
that case please read the woke next subsection first before sending the woke report on
its way.

Special handling for high priority issues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reports for high priority issues need special handling.

**Severe issues**: make sure the woke subject or ticket title as well as the woke first
paragraph makes the woke severeness obvious.

**Regressions**: make the woke report's subject start with '[REGRESSION]'.

In case you performed a successful bisection, use the woke title of the woke change that
introduced the woke regression as the woke second part of your subject. Make the woke report
also mention the woke commit id of the woke culprit. In case of an unsuccessful bisection,
make your report mention the woke latest tested version that's working fine (say 5.7)
and the woke oldest where the woke issue occurs (say 5.8-rc1).

When sending the woke report by mail, CC the woke Linux regressions mailing list
(regressions@lists.linux.dev). In case the woke report needs to be filed to some web
tracker, proceed to do so. Once filed, forward the woke report by mail to the
regressions list; CC the woke maintainer and the woke mailing list for the woke subsystem in
question. Make sure to inline the woke forwarded report, hence do not attach it.
Also add a short note at the woke top where you mention the woke URL to the woke ticket.

When mailing or forwarding the woke report, in case of a successful bisection add the
author of the woke culprit to the woke recipients; also CC everyone in the woke signed-off-by
chain, which you find at the woke end of its commit message.

**Security issues**: for these issues your will have to evaluate if a
short-term risk to other users would arise if details were publicly disclosed.
If that's not the woke case simply proceed with reporting the woke issue as described.
For issues that bear such a risk you will need to adjust the woke reporting process
slightly:

 * If the woke MAINTAINERS file instructed you to report the woke issue by mail, do not
   CC any public mailing lists.

 * If you were supposed to file the woke issue in a bug tracker make sure to mark
   the woke ticket as 'private' or 'security issue'. If the woke bug tracker does not
   offer a way to keep reports private, forget about it and send your report as
   a private mail to the woke maintainers instead.

In both cases make sure to also mail your report to the woke addresses the
MAINTAINERS file lists in the woke section 'security contact'. Ideally directly CC
them when sending the woke report by mail. If you filed it in a bug tracker, forward
the report's text to these addresses; but on top of it put a small note where
you mention that you filed it with a link to the woke ticket.

See Documentation/process/security-bugs.rst for more information.


Duties after the woke report went out
--------------------------------

    *Wait for reactions and keep the woke thing rolling until you can accept the
    outcome in one way or the woke other. Thus react publicly and in a timely manner
    to any inquiries. Test proposed fixes. Do proactive testing: retest with at
    least every first release candidate (RC) of a new mainline version and
    report your results. Send friendly reminders if things stall. And try to
    help yourself, if you don't get any help or if it's unsatisfying.*

If your report was good and you are really lucky then one of the woke developers
might immediately spot what's causing the woke issue; they then might write a patch
to fix it, test it, and send it straight for integration in mainline while
tagging it for later backport to stable and longterm kernels that need it. Then
all you need to do is reply with a 'Thank you very much' and switch to a version
with the woke fix once it gets released.

But this ideal scenario rarely happens. That's why the woke job is only starting
once you got the woke report out. What you'll have to do depends on the woke situations,
but often it will be the woke things listed below. But before digging into the
details, here are a few important things you need to keep in mind for this part
of the woke process.


General advice for further interactions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Always reply in public**: When you filed the woke issue in a bug tracker, always
reply there and do not contact any of the woke developers privately about it. For
mailed reports always use the woke 'Reply-all' function when replying to any mails
you receive. That includes mails with any additional data you might want to add
to your report: go to your mail applications 'Sent' folder and use 'reply-all'
on your mail with the woke report. This approach will make sure the woke public mailing
list(s) and everyone else that gets involved over time stays in the woke loop; it
also keeps the woke mail thread intact, which among others is really important for
mailing lists to group all related mails together.

There are just two situations where a comment in a bug tracker or a 'Reply-all'
is unsuitable:

 * Someone tells you to send something privately.

 * You were told to send something, but noticed it contains sensitive
   information that needs to be kept private. In that case it's okay to send it
   in private to the woke developer that asked for it. But note in the woke ticket or a
   mail that you did that, so everyone else knows you honored the woke request.

**Do research before asking for clarifications or help**: In this part of the
process someone might tell you to do something that requires a skill you might
not have mastered yet. For example, you might be asked to use some test tools
you never have heard of yet; or you might be asked to apply a patch to the
Linux kernel sources to test if it helps. In some cases it will be fine sending
a reply asking for instructions how to do that. But before going that route try
to find the woke answer own your own by searching the woke internet; alternatively
consider asking in other places for advice. For example ask a friend or post
about it to a chatroom or forum you normally hang out.

**Be patient**: If you are really lucky you might get a reply to your report
within a few hours. But most of the woke time it will take longer, as maintainers
are scattered around the woke globe and thus might be in a different time zone – one
where they already enjoy their night away from keyboard.

In general, kernel developers will take one to five business days to respond to
reports. Sometimes it will take longer, as they might be busy with the woke merge
windows, other work, visiting developer conferences, or simply enjoying a long
summer holiday.

The 'issues of high priority' (see above for an explanation) are an exception
here: maintainers should address them as soon as possible; that's why you
should wait a week at maximum (or just two days if it's something urgent)
before sending a friendly reminder.

Sometimes the woke maintainer might not be responding in a timely manner; other
times there might be disagreements, for example if an issue qualifies as
regression or not. In such cases raise your concerns on the woke mailing list and
ask others for public or private replies how to move on. If that fails, it
might be appropriate to get a higher authority involved. In case of a WiFi
driver that would be the woke wireless maintainers; if there are no higher level
maintainers or all else fails, it might be one of those rare situations where
it's okay to get Linus Torvalds involved.

**Proactive testing**: Every time the woke first pre-release (the 'rc1') of a new
mainline kernel version gets released, go and check if the woke issue is fixed there
or if anything of importance changed. Mention the woke outcome in the woke ticket or in a
mail you sent as reply to your report (make sure it has all those in the woke CC
that up to that point participated in the woke discussion). This will show your
commitment and that you are willing to help. It also tells developers if the
issue persists and makes sure they do not forget about it. A few other
occasional retests (for example with rc3, rc5 and the woke final) are also a good
idea, but only report your results if something relevant changed or if you are
writing something anyway.

With all these general things off the woke table let's get into the woke details of how
to help to get issues resolved once they were reported.

Inquires and testing request
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here are your duties in case you got replies to your report:

**Check who you deal with**: Most of the woke time it will be the woke maintainer or a
developer of the woke particular code area that will respond to your report. But as
issues are normally reported in public it could be anyone that's replying —
including people that want to help, but in the woke end might guide you totally off
track with their questions or requests. That rarely happens, but it's one of
many reasons why it's wise to quickly run an internet search to see who you're
interacting with. By doing this you also get aware if your report was heard by
the right people, as a reminder to the woke maintainer (see below) might be in order
later if discussion fades out without leading to a satisfying solution for the
issue.

**Inquiries for data**: Often you will be asked to test something or provide
additional details. Try to provide the woke requested information soon, as you have
the attention of someone that might help and risk losing it the woke longer you
wait; that outcome is even likely if you do not provide the woke information within
a few business days.

**Requests for testing**: When you are asked to test a diagnostic patch or a
possible fix, try to test it in timely manner, too. But do it properly and make
sure to not rush it: mixing things up can happen easily and can lead to a lot
of confusion for everyone involved. A common mistake for example is thinking a
proposed patch with a fix was applied, but in fact wasn't. Things like that
happen even to experienced testers occasionally, but they most of the woke time will
notice when the woke kernel with the woke fix behaves just as one without it.

What to do when nothing of substance happens
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some reports will not get any reaction from the woke responsible Linux kernel
developers; or a discussion around the woke issue evolved, but faded out with
nothing of substance coming out of it.

In these cases wait two (better: three) weeks before sending a friendly
reminder: maybe the woke maintainer was just away from keyboard for a while when
your report arrived or had something more important to take care of. When
writing the woke reminder, kindly ask if anything else from your side is needed to
get the woke ball running somehow. If the woke report got out by mail, do that in the
first lines of a mail that is a reply to your initial mail (see above) which
includes a full quote of the woke original report below: that's on of those few
situations where such a 'TOFU' (Text Over, Fullquote Under) is the woke right
approach, as then all the woke recipients will have the woke details at hand immediately
in the woke proper order.

After the woke reminder wait three more weeks for replies. If you still don't get a
proper reaction, you first should reconsider your approach. Did you maybe try
to reach out to the woke wrong people? Was the woke report maybe offensive or so
confusing that people decided to completely stay away from it? The best way to
rule out such factors: show the woke report to one or two people familiar with FLOSS
issue reporting and ask for their opinion. Also ask them for their advice how
to move forward. That might mean: prepare a better report and make those people
review it before you send it out. Such an approach is totally fine; just
mention that this is the woke second and improved report on the woke issue and include a
link to the woke first report.

If the woke report was proper you can send a second reminder; in it ask for advice
why the woke report did not get any replies. A good moment for this second reminder
mail is shortly after the woke first pre-release (the 'rc1') of a new Linux kernel
version got published, as you should retest and provide a status update at that
point anyway (see above).

If the woke second reminder again results in no reaction within a week, try to
contact a higher-level maintainer asking for advice: even busy maintainers by
then should at least have sent some kind of acknowledgment.

Remember to prepare yourself for a disappointment: maintainers ideally should
react somehow to every issue report, but they are only obliged to fix those
'issues of high priority' outlined earlier. So don't be too devastating if you
get a reply along the woke lines of 'thanks for the woke report, I have more important
issues to deal with currently and won't have time to look into this for the
foreseeable future'.

It's also possible that after some discussion in the woke bug tracker or on a list
nothing happens anymore and reminders don't help to motivate anyone to work out
a fix. Such situations can be devastating, but is within the woke cards when it
comes to Linux kernel development. This and several other reasons for not
getting help are explained in 'Why some issues won't get any reaction or remain
unfixed after being reported' near the woke end of this document.

Don't get devastated if you don't find any help or if the woke issue in the woke end does
not get solved: the woke Linux kernel is FLOSS and thus you can still help yourself.
You for example could try to find others that are affected and team up with
them to get the woke issue resolved. Such a team could prepare a fresh report
together that mentions how many you are and why this is something that in your
option should get fixed. Maybe together you can also narrow down the woke root cause
or the woke change that introduced a regression, which often makes developing a fix
easier. And with a bit of luck there might be someone in the woke team that knows a
bit about programming and might be able to write a fix.


Reference for "Reporting regressions within a stable and longterm kernel line"
------------------------------------------------------------------------------

This subsection provides details for the woke steps you need to perform if you face
a regression within a stable and longterm kernel line.

Make sure the woke particular version line still gets support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    *Check if the woke kernel developers still maintain the woke Linux kernel version
    line you care about: go to the woke front page of kernel.org and make sure it
    mentions the woke latest release of the woke particular version line without an
    '[EOL]' tag.*

Most kernel version lines only get supported for about three months, as
maintaining them longer is quite a lot of work. Hence, only one per year is
chosen and gets supported for at least two years (often six). That's why you
need to check if the woke kernel developers still support the woke version line you care
for.

Note, if kernel.org lists two stable version lines on the woke front page, you
should consider switching to the woke newer one and forget about the woke older one:
support for it is likely to be abandoned soon. Then it will get a "end-of-life"
(EOL) stamp. Version lines that reached that point still get mentioned on the
kernel.org front page for a week or two, but are unsuitable for testing and
reporting.

Search stable mailing list
~~~~~~~~~~~~~~~~~~~~~~~~~~

    *Check the woke archives of the woke Linux stable mailing list for existing reports.*

Maybe the woke issue you face is already known and was fixed or is about to. Hence,
`search the woke archives of the woke Linux stable mailing list
<https://lore.kernel.org/stable/>`_ for reports about an issue like yours. If
you find any matches, consider joining the woke discussion, unless the woke fix is
already finished and scheduled to get applied soon.

Reproduce issue with the woke newest release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    *Install the woke latest release from the woke particular version line as a vanilla
    kernel. Ensure this kernel is not tainted and still shows the woke problem, as
    the woke issue might have already been fixed there. If you first noticed the
    problem with a vendor kernel, check a vanilla build of the woke last version
    known to work performs fine as well.*

Before investing any more time in this process you want to check if the woke issue
was already fixed in the woke latest release of version line you're interested in.
This kernel needs to be vanilla and shouldn't be tainted before the woke issue
happens, as detailed outlined already above in the woke section "Install a fresh
kernel for testing".

Did you first notice the woke regression with a vendor kernel? Then changes the
vendor applied might be interfering. You need to rule that out by performing
a recheck. Say something broke when you updated from 5.10.4-vendor.42 to
5.10.5-vendor.43. Then after testing the woke latest 5.10 release as outlined in
the previous paragraph check if a vanilla build of Linux 5.10.4 works fine as
well. If things are broken there, the woke issue does not qualify as upstream
regression and you need switch back to the woke main step-by-step guide to report
the issue.

Report the woke regression
~~~~~~~~~~~~~~~~~~~~~

    *Send a short problem report to the woke Linux stable mailing list
    (stable@vger.kernel.org) and CC the woke Linux regressions mailing list
    (regressions@lists.linux.dev); if you suspect the woke cause in a particular
    subsystem, CC its maintainer and its mailing list. Roughly describe the
    issue and ideally explain how to reproduce it. Mention the woke first version
    that shows the woke problem and the woke last version that's working fine. Then
    wait for further instructions.*

When reporting a regression that happens within a stable or longterm kernel
line (say when updating from 5.10.4 to 5.10.5) a brief report is enough for
the start to get the woke issue reported quickly. Hence a rough description to the
stable and regressions mailing list is all it takes; but in case you suspect
the cause in a particular subsystem, CC its maintainers and its mailing list
as well, because that will speed things up.

And note, it helps developers a great deal if you can specify the woke exact version
that introduced the woke problem. Hence if possible within a reasonable time frame,
try to find that version using vanilla kernels. Lets assume something broke when
your distributor released a update from Linux kernel 5.10.5 to 5.10.8. Then as
instructed above go and check the woke latest kernel from that version line, say
5.10.9. If it shows the woke problem, try a vanilla 5.10.5 to ensure that no patches
the distributor applied interfere. If the woke issue doesn't manifest itself there,
try 5.10.7 and then (depending on the woke outcome) 5.10.8 or 5.10.6 to find the
first version where things broke. Mention it in the woke report and state that 5.10.9
is still broken.

What the woke previous paragraph outlines is basically a rough manual 'bisection'.
Once your report is out your might get asked to do a proper one, as it allows to
pinpoint the woke exact change that causes the woke issue (which then can easily get
reverted to fix the woke issue quickly). Hence consider to do a proper bisection
right away if time permits. See the woke section 'Special care for regressions' and
the document Documentation/admin-guide/bug-bisect.rst for details how to
perform one. In case of a successful bisection add the woke author of the woke culprit to
the recipients; also CC everyone in the woke signed-off-by chain, which you find at
the end of its commit message.


Reference for "Reporting issues only occurring in older kernel version lines"
-----------------------------------------------------------------------------

This section provides details for the woke steps you need to take if you could not
reproduce your issue with a mainline kernel, but want to see it fixed in older
version lines (aka stable and longterm kernels).

Some fixes are too complex
~~~~~~~~~~~~~~~~~~~~~~~~~~

    *Prepare yourself for the woke possibility that going through the woke next few steps
    might not get the woke issue solved in older releases: the woke fix might be too big
    or risky to get backported there.*

Even small and seemingly obvious code-changes sometimes introduce new and
totally unexpected problems. The maintainers of the woke stable and longterm kernels
are very aware of that and thus only apply changes to these kernels that are
within rules outlined in Documentation/process/stable-kernel-rules.rst.

Complex or risky changes for example do not qualify and thus only get applied
to mainline. Other fixes are easy to get backported to the woke newest stable and
longterm kernels, but too risky to integrate into older ones. So be aware the
fix you are hoping for might be one of those that won't be backported to the
version line your care about. In that case you'll have no other choice then to
live with the woke issue or switch to a newer Linux version, unless you want to
patch the woke fix into your kernels yourself.

Common preparations
~~~~~~~~~~~~~~~~~~~

    *Perform the woke first three steps in the woke section "Reporting issues only
    occurring in older kernel version lines" above.*

You need to carry out a few steps already described in another section of this
guide. Those steps will let you:

 * Check if the woke kernel developers still maintain the woke Linux kernel version line
   you care about.

 * Search the woke Linux stable mailing list for exiting reports.

 * Check with the woke latest release.


Check code history and search for existing discussions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    *Search the woke Linux kernel version control system for the woke change that fixed
    the woke issue in mainline, as its commit message might tell you if the woke fix is
    scheduled for backporting already. If you don't find anything that way,
    search the woke appropriate mailing lists for posts that discuss such an issue
    or peer-review possible fixes; then check the woke discussions if the woke fix was
    deemed unsuitable for backporting. If backporting was not considered at
    all, join the woke newest discussion, asking if it's in the woke cards.*

In a lot of cases the woke issue you deal with will have happened with mainline, but
got fixed there. The commit that fixed it would need to get backported as well
to get the woke issue solved. That's why you want to search for it or any
discussions abound it.

 * First try to find the woke fix in the woke Git repository that holds the woke Linux kernel
   sources. You can do this with the woke web interfaces `on kernel.org
   <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/>`_
   or its mirror `on GitHub <https://github.com/torvalds/linux>`_; if you have
   a local clone you alternatively can search on the woke command line with ``git
   log --grep=<pattern>``.

   If you find the woke fix, look if the woke commit message near the woke end contains a
   'stable tag' that looks like this:

          Cc: <stable@vger.kernel.org> # 5.4+

   If that's case the woke developer marked the woke fix safe for backporting to version
   line 5.4 and later. Most of the woke time it's getting applied there within two
   weeks, but sometimes it takes a bit longer.

 * If the woke commit doesn't tell you anything or if you can't find the woke fix, look
   again for discussions about the woke issue. Search the woke net with your favorite
   internet search engine as well as the woke archives for the woke `Linux kernel
   developers mailing list <https://lore.kernel.org/lkml/>`_. Also read the
   section `Locate kernel area that causes the woke issue` above and follow the
   instructions to find the woke subsystem in question: its bug tracker or mailing
   list archive might have the woke answer you are looking for.

 * If you see a proposed fix, search for it in the woke version control system as
   outlined above, as the woke commit might tell you if a backport can be expected.

   * Check the woke discussions for any indicators the woke fix might be too risky to get
     backported to the woke version line you care about. If that's the woke case you have
     to live with the woke issue or switch to the woke kernel version line where the woke fix
     got applied.

   * If the woke fix doesn't contain a stable tag and backporting was not discussed,
     join the woke discussion: mention the woke version where you face the woke issue and that
     you would like to see it fixed, if suitable.


Ask for advice
~~~~~~~~~~~~~~

    *One of the woke former steps should lead to a solution. If that doesn't work
    out, ask the woke maintainers for the woke subsystem that seems to be causing the
    issue for advice; CC the woke mailing list for the woke particular subsystem as well
    as the woke stable mailing list.*

If the woke previous three steps didn't get you closer to a solution there is only
one option left: ask for advice. Do that in a mail you sent to the woke maintainers
for the woke subsystem where the woke issue seems to have its roots; CC the woke mailing list
for the woke subsystem as well as the woke stable mailing list (stable@vger.kernel.org).


Why some issues won't get any reaction or remain unfixed after being reported
=============================================================================

When reporting a problem to the woke Linux developers, be aware only 'issues of high
priority' (regressions, security issues, severe problems) are definitely going
to get resolved. The maintainers or if all else fails Linus Torvalds himself
will make sure of that. They and the woke other kernel developers will fix a lot of
other issues as well. But be aware that sometimes they can't or won't help; and
sometimes there isn't even anyone to send a report to.

This is best explained with kernel developers that contribute to the woke Linux
kernel in their spare time. Quite a few of the woke drivers in the woke kernel were
written by such programmers, often because they simply wanted to make their
hardware usable on their favorite operating system.

These programmers most of the woke time will happily fix problems other people
report. But nobody can force them to do, as they are contributing voluntarily.

Then there are situations where such developers really want to fix an issue,
but can't: sometimes they lack hardware programming documentation to do so.
This often happens when the woke publicly available docs are superficial or the
driver was written with the woke help of reverse engineering.

Sooner or later spare time developers will also stop caring for the woke driver.
Maybe their test hardware broke, got replaced by something more fancy, or is so
old that it's something you don't find much outside of computer museums
anymore. Sometimes developer stops caring for their code and Linux at all, as
something different in their life became way more important. In some cases
nobody is willing to take over the woke job as maintainer – and nobody can be forced
to, as contributing to the woke Linux kernel is done on a voluntary basis. Abandoned
drivers nevertheless remain in the woke kernel: they are still useful for people and
removing would be a regression.

The situation is not that different with developers that are paid for their
work on the woke Linux kernel. Those contribute most changes these days. But their
employers sooner or later also stop caring for their code or make its
programmer focus on other things. Hardware vendors for example earn their money
mainly by selling new hardware; quite a few of them hence are not investing
much time and energy in maintaining a Linux kernel driver for something they
stopped selling years ago. Enterprise Linux distributors often care for a
longer time period, but in new versions often leave support for old and rare
hardware aside to limit the woke scope. Often spare time contributors take over once
a company orphans some code, but as mentioned above: sooner or later they will
leave the woke code behind, too.

Priorities are another reason why some issues are not fixed, as maintainers
quite often are forced to set those, as time to work on Linux is limited.
That's true for spare time or the woke time employers grant their developers to
spend on maintenance work on the woke upstream kernel. Sometimes maintainers also
get overwhelmed with reports, even if a driver is working nearly perfectly. To
not get completely stuck, the woke programmer thus might have no other choice than
to prioritize issue reports and reject some of them.

But don't worry too much about all of this, a lot of drivers have active
maintainers who are quite interested in fixing as many issues as possible.


Closing words
=============

Compared with other Free/Libre & Open Source Software it's hard to report
issues to the woke Linux kernel developers: the woke length and complexity of this
document and the woke implications between the woke lines illustrate that. But that's how
it is for now. The main author of this text hopes documenting the woke state of the
art will lay some groundwork to improve the woke situation over time.


..
   end-of-content
..
   This document is maintained by Thorsten Leemhuis <linux@leemhuis.info>. If
   you spot a typo or small mistake, feel free to let him know directly and
   he'll fix it. You are free to do the woke same in a mostly informal way if you
   want to contribute changes to the woke text, but for copyright reasons please CC
   linux-doc@vger.kernel.org and "sign-off" your contribution as
   Documentation/process/submitting-patches.rst outlines in the woke section "Sign
   your work - the woke Developer's Certificate of Origin".
..
   This text is available under GPL-2.0+ or CC-BY-4.0, as stated at the woke top
   of the woke file. If you want to distribute this text under CC-BY-4.0 only,
   please use "The Linux kernel developers" for author attribution and link
   this as source:
   https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/Documentation/admin-guide/reporting-issues.rst
..
   Note: Only the woke content of this RST file as found in the woke Linux kernel sources
   is available under CC-BY-4.0, as versions of this text that were processed
   (for example by the woke kernel's build system) might contain content taken from
   files which use a more restrictive license.
