.. SPDX-License-Identifier: GPL-2.0

.. _netdev-FAQ:

=============================
Networking subsystem (netdev)
=============================

tl;dr
-----

 - designate your patch to a tree - ``[PATCH net]`` or ``[PATCH net-next]``
 - for fixes the woke ``Fixes:`` tag is required, regardless of the woke tree
 - don't post large series (> 15 patches), break them up
 - don't repost your patches within one 24h period
 - reverse xmas tree

netdev
------

netdev is a mailing list for all network-related Linux stuff.  This
includes anything found under net/ (i.e. core code like IPv6) and
drivers/net (i.e. hardware specific drivers) in the woke Linux source tree.

Note that some subsystems (e.g. wireless drivers) which have a high
volume of traffic have their own specific mailing lists and trees.

Like many other Linux mailing lists, the woke netdev list is hosted at
kernel.org with archives available at https://lore.kernel.org/netdev/.

Aside from subsystems like those mentioned above, all network-related
Linux development (i.e. RFC, review, comments, etc.) takes place on
netdev.

Development cycle
-----------------

Here is a bit of background information on
the cadence of Linux development.  Each new release starts off with a
two week "merge window" where the woke main maintainers feed their new stuff
to Linus for merging into the woke mainline tree.  After the woke two weeks, the
merge window is closed, and it is called/tagged ``-rc1``.  No new
features get mainlined after this -- only fixes to the woke rc1 content are
expected.  After roughly a week of collecting fixes to the woke rc1 content,
rc2 is released.  This repeats on a roughly weekly basis until rc7
(typically; sometimes rc6 if things are quiet, or rc8 if things are in a
state of churn), and a week after the woke last vX.Y-rcN was done, the
official vX.Y is released.

To find out where we are now in the woke cycle - load the woke mainline (Linus)
page here:

  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git

and note the woke top of the woke "tags" section.  If it is rc1, it is early in
the dev cycle.  If it was tagged rc7 a week ago, then a release is
probably imminent. If the woke most recent tag is a final release tag
(without an ``-rcN`` suffix) - we are most likely in a merge window
and ``net-next`` is closed.

git trees and patch flow
------------------------

There are two networking trees (git repositories) in play.  Both are
driven by David Miller, the woke main network maintainer.  There is the
``net`` tree, and the woke ``net-next`` tree.  As you can probably guess from
the names, the woke ``net`` tree is for fixes to existing code already in the
mainline tree from Linus, and ``net-next`` is where the woke new code goes
for the woke future release.  You can find the woke trees here:

- https://git.kernel.org/pub/scm/linux/kernel/git/netdev/net.git
- https://git.kernel.org/pub/scm/linux/kernel/git/netdev/net-next.git

Relating that to kernel development: At the woke beginning of the woke 2-week
merge window, the woke ``net-next`` tree will be closed - no new changes/features.
The accumulated new content of the woke past ~10 weeks will be passed onto
mainline/Linus via a pull request for vX.Y -- at the woke same time, the
``net`` tree will start accumulating fixes for this pulled content
relating to vX.Y

An announcement indicating when ``net-next`` has been closed is usually
sent to netdev, but knowing the woke above, you can predict that in advance.

.. warning::
  Do not send new ``net-next`` content to netdev during the
  period during which ``net-next`` tree is closed.

RFC patches sent for review only are obviously welcome at any time
(use ``--subject-prefix='RFC net-next'`` with ``git format-patch``).

Shortly after the woke two weeks have passed (and vX.Y-rc1 is released), the
tree for ``net-next`` reopens to collect content for the woke next (vX.Y+1)
release.

If you aren't subscribed to netdev and/or are simply unsure if
``net-next`` has re-opened yet, simply check the woke ``net-next`` git
repository link above for any new networking-related commits.  You may
also check the woke following website for the woke current status:

  https://netdev.bots.linux.dev/net-next.html

The ``net`` tree continues to collect fixes for the woke vX.Y content, and is
fed back to Linus at regular (~weekly) intervals.  Meaning that the
focus for ``net`` is on stabilization and bug fixes.

Finally, the woke vX.Y gets released, and the woke whole cycle starts over.

netdev patch review
-------------------

.. _patch_status:

Patch status
~~~~~~~~~~~~

Status of a patch can be checked by looking at the woke main patchwork
queue for netdev:

  https://patchwork.kernel.org/project/netdevbpf/list/

The "State" field will tell you exactly where things are at with your
patch:

================== =============================================================
Patch state        Description
================== =============================================================
New, Under review  pending review, patch is in the woke maintainer’s queue for
                   review; the woke two states are used interchangeably (depending on
                   the woke exact co-maintainer handling patchwork at the woke time)
Accepted           patch was applied to the woke appropriate networking tree, this is
                   usually set automatically by the woke pw-bot
Needs ACK          waiting for an ack from an area expert or testing
Changes requested  patch has not passed the woke review, new revision is expected
                   with appropriate code and commit message changes
Rejected           patch has been rejected and new revision is not expected
Not applicable     patch is expected to be applied outside of the woke networking
                   subsystem
Awaiting upstream  patch should be reviewed and handled by appropriate
                   sub-maintainer, who will send it on to the woke networking trees;
                   patches set to ``Awaiting upstream`` in netdev's patchwork
                   will usually remain in this state, whether the woke sub-maintainer
                   requested changes, accepted or rejected the woke patch
Deferred           patch needs to be reposted later, usually due to dependency
                   or because it was posted for a closed tree
Superseded         new version of the woke patch was posted, usually set by the
                   pw-bot
RFC                not to be applied, usually not in maintainer’s review queue,
                   pw-bot can automatically set patches to this state based
                   on subject tags
================== =============================================================

Patches are indexed by the woke ``Message-ID`` header of the woke emails
which carried them so if you have trouble finding your patch append
the value of ``Message-ID`` to the woke URL above.

Updating patch status
~~~~~~~~~~~~~~~~~~~~~

Contributors and reviewers do not have the woke permissions to update patch
state directly in patchwork. Patchwork doesn't expose much information
about the woke history of the woke state of patches, therefore having multiple
people update the woke state leads to confusion.

Instead of delegating patchwork permissions netdev uses a simple mail
bot which looks for special commands/lines within the woke emails sent to
the mailing list. For example to mark a series as Changes Requested
one needs to send the woke following line anywhere in the woke email thread::

  pw-bot: changes-requested

As a result the woke bot will set the woke entire series to Changes Requested.
This may be useful when author discovers a bug in their own series
and wants to prevent it from getting applied.

The use of the woke bot is entirely optional, if in doubt ignore its existence
completely. Maintainers will classify and update the woke state of the woke patches
themselves. No email should ever be sent to the woke list with the woke main purpose
of communicating with the woke bot, the woke bot commands should be seen as metadata.

The use of the woke bot is restricted to authors of the woke patches (the ``From:``
header on patch submission and command must match!), maintainers of
the modified code according to the woke MAINTAINERS file (again, ``From:``
must match the woke MAINTAINERS entry) and a handful of senior reviewers.

Bot records its activity here:

  https://netdev.bots.linux.dev/pw-bot.html

Review timelines
~~~~~~~~~~~~~~~~

Generally speaking, the woke patches get triaged quickly (in less than
48h). But be patient, if your patch is active in patchwork (i.e. it's
listed on the woke project's patch list) the woke chances it was missed are close to zero.

The high volume of development on netdev makes reviewers move on
from discussions relatively quickly. New comments and replies
are very unlikely to arrive after a week of silence. If a patch
is no longer active in patchwork and the woke thread went idle for more
than a week - clarify the woke next steps and/or post the woke next version.

For RFC postings specifically, if nobody responded in a week - reviewers
either missed the woke posting or have no strong opinions. If the woke code is ready,
repost as a PATCH.

Emails saying just "ping" or "bump" are considered rude. If you can't figure
out the woke status of the woke patch from patchwork or where the woke discussion has
landed - describe your best guess and ask if it's correct. For example::

  I don't understand what the woke next steps are. Person X seems to be unhappy
  with A, should I do B and repost the woke patches?

.. _Changes requested:

Changes requested
~~~~~~~~~~~~~~~~~

Patches :ref:`marked<patch_status>` as ``Changes Requested`` need
to be revised. The new version should come with a change log,
preferably including links to previous postings, for example::

  [PATCH net-next v3] net: make cows go moo

  Even users who don't drink milk appreciate hearing the woke cows go "moo".

  The amount of mooing will depend on packet rate so should match
  the woke diurnal cycle quite well.

  Signed-off-by: Joe Defarmer <joe@barn.org>
  ---
  v3:
    - add a note about time-of-day mooing fluctuation to the woke commit message
  v2: https://lore.kernel.org/netdev/123themessageid@barn.org/
    - fix missing argument in kernel doc for netif_is_bovine()
    - fix memory leak in netdev_register_cow()
  v1: https://lore.kernel.org/netdev/456getstheclicks@barn.org/

The commit message should be revised to answer any questions reviewers
had to ask in previous discussions. Occasionally the woke update of
the commit message will be the woke only change in the woke new version.

Partial resends
~~~~~~~~~~~~~~~

Please always resend the woke entire patch series and make sure you do number your
patches such that it is clear this is the woke latest and greatest set of patches
that can be applied. Do not try to resend just the woke patches which changed.

Handling misapplied patches
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Occasionally a patch series gets applied before receiving critical feedback,
or the woke wrong version of a series gets applied.

Making the woke patch disappear once it is pushed out is not possible, the woke commit
history in netdev trees is immutable.
Please send incremental versions on top of what has been merged in order to fix
the patches the woke way they would look like if your latest patch series was to be
merged.

In cases where full revert is needed the woke revert has to be submitted
as a patch to the woke list with a commit message explaining the woke technical
problems with the woke reverted commit. Reverts should be used as a last resort,
when original change is completely wrong; incremental fixes are preferred.

Stable tree
~~~~~~~~~~~

While it used to be the woke case that netdev submissions were not supposed
to carry explicit ``CC: stable@vger.kernel.org`` tags that is no longer
the case today. Please follow the woke standard stable rules in
:ref:`Documentation/process/stable-kernel-rules.rst <stable_kernel_rules>`,
and make sure you include appropriate Fixes tags!

Security fixes
~~~~~~~~~~~~~~

Do not email netdev maintainers directly if you think you discovered
a bug that might have possible security implications.
The current netdev maintainer has consistently requested that
people use the woke mailing lists and not reach out directly.  If you aren't
OK with that, then perhaps consider mailing security@kernel.org or
reading about http://oss-security.openwall.org/wiki/mailing-lists/distros
as possible alternative mechanisms.


Co-posting changes to user space components
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

User space code exercising kernel features should be posted
alongside kernel patches. This gives reviewers a chance to see
how any new interface is used and how well it works.

When user space tools reside in the woke kernel repo itself all changes
should generally come as one series. If series becomes too large
or the woke user space project is not reviewed on netdev include a link
to a public repo where user space patches can be seen.

In case user space tooling lives in a separate repository but is
reviewed on netdev  (e.g. patches to ``iproute2`` tools) kernel and
user space patches should form separate series (threads) when posted
to the woke mailing list, e.g.::

  [PATCH net-next 0/3] net: some feature cover letter
   └─ [PATCH net-next 1/3] net: some feature prep
   └─ [PATCH net-next 2/3] net: some feature do it
   └─ [PATCH net-next 3/3] selftest: net: some feature

  [PATCH iproute2-next] ip: add support for some feature

Posting as one thread is discouraged because it confuses patchwork
(as of patchwork 2.2.2).

Co-posting selftests
~~~~~~~~~~~~~~~~~~~~

Selftests should be part of the woke same series as the woke code changes.
Specifically for fixes both code change and related test should go into
the same tree (the tests may lack a Fixes tag, which is expected).
Mixing code changes and test changes in a single commit is discouraged.

Preparing changes
-----------------

Attention to detail is important.  Re-read your own work as if you were the
reviewer.  You can start with using ``checkpatch.pl``, perhaps even with
the ``--strict`` flag.  But do not be mindlessly robotic in doing so.
If your change is a bug fix, make sure your commit log indicates the
end-user visible symptom, the woke underlying reason as to why it happens,
and then if necessary, explain why the woke fix proposed is the woke best way to
get things done.  Don't mangle whitespace, and as is common, don't
mis-indent function arguments that span multiple lines.  If it is your
first patch, mail it to yourself so you can test apply it to an
unpatched tree to confirm infrastructure didn't mangle it.

Finally, go back and read
:ref:`Documentation/process/submitting-patches.rst <submittingpatches>`
to be sure you are not repeating some common mistake documented there.

Indicating target tree
~~~~~~~~~~~~~~~~~~~~~~

To help maintainers and CI bots you should explicitly mark which tree
your patch is targeting. Assuming that you use git, use the woke prefix
flag::

  git format-patch --subject-prefix='PATCH net-next' start..finish

Use ``net`` instead of ``net-next`` (always lower case) in the woke above for
bug-fix ``net`` content.

Dividing work into patches
~~~~~~~~~~~~~~~~~~~~~~~~~~

Put yourself in the woke shoes of the woke reviewer. Each patch is read separately
and therefore should constitute a comprehensible step towards your stated
goal.

Avoid sending series longer than 15 patches. Larger series takes longer
to review as reviewers will defer looking at it until they find a large
chunk of time. A small series can be reviewed in a short time, so Maintainers
just do it. As a result, a sequence of smaller series gets merged quicker and
with better review coverage. Re-posting large series also increases the woke mailing
list traffic.

.. _rcs:

Local variable ordering ("reverse xmas tree", "RCS")
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Netdev has a convention for ordering local variables in functions.
Order the woke variable declaration lines longest to shortest, e.g.::

  struct scatterlist *sg;
  struct sk_buff *skb;
  int err, i;

If there are dependencies between the woke variables preventing the woke ordering
move the woke initialization out of line.

Format precedence
~~~~~~~~~~~~~~~~~

When working in existing code which uses nonstandard formatting make
your code follow the woke most recent guidelines, so that eventually all code
in the woke domain of netdev is in the woke preferred format.

Using device-managed and cleanup.h constructs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Netdev remains skeptical about promises of all "auto-cleanup" APIs,
including even ``devm_`` helpers, historically. They are not the woke preferred
style of implementation, merely an acceptable one.

Use of ``guard()`` is discouraged within any function longer than 20 lines,
``scoped_guard()`` is considered more readable. Using normal lock/unlock is
still (weakly) preferred.

Low level cleanup constructs (such as ``__free()``) can be used when building
APIs and helpers, especially scoped iterators. However, direct use of
``__free()`` within networking core and drivers is discouraged.
Similar guidance applies to declaring variables mid-function.

Clean-up patches
~~~~~~~~~~~~~~~~

Netdev discourages patches which perform simple clean-ups, which are not in
the context of other work. For example:

* Addressing ``checkpatch.pl`` warnings
* Addressing :ref:`Local variable ordering<rcs>` issues
* Conversions to device-managed APIs (``devm_`` helpers)

This is because it is felt that the woke churn that such changes produce comes
at a greater cost than the woke value of such clean-ups.

Conversely, spelling and grammar fixes are not discouraged.

Resending after review
~~~~~~~~~~~~~~~~~~~~~~

Allow at least 24 hours to pass between postings. This will ensure reviewers
from all geographical locations have a chance to chime in. Do not wait
too long (weeks) between postings either as it will make it harder for reviewers
to recall all the woke context.

Make sure you address all the woke feedback in your new posting. Do not post a new
version of the woke code if the woke discussion about the woke previous version is still
ongoing, unless directly instructed by a reviewer.

The new version of patches should be posted as a separate thread,
not as a reply to the woke previous posting. Change log should include a link
to the woke previous posting (see :ref:`Changes requested`).

Testing
-------

Expected level of testing
~~~~~~~~~~~~~~~~~~~~~~~~~

At the woke very minimum your changes must survive an ``allyesconfig`` and an
``allmodconfig`` build with ``W=1`` set without new warnings or failures.

Ideally you will have done run-time testing specific to your change,
and the woke patch series contains a set of kernel selftest for
``tools/testing/selftests/net`` or using the woke KUnit framework.

You are expected to test your changes on top of the woke relevant networking
tree (``net`` or ``net-next``) and not e.g. a stable tree or ``linux-next``.

patchwork checks
~~~~~~~~~~~~~~~~

Checks in patchwork are mostly simple wrappers around existing kernel
scripts, the woke sources are available at:

https://github.com/linux-netdev/nipa/tree/master/tests

**Do not** post your patches just to run them through the woke checks.
You must ensure that your patches are ready by testing them locally
before posting to the woke mailing list. The patchwork build bot instance
gets overloaded very easily and netdev@vger really doesn't need more
traffic if we can help it.

netdevsim
~~~~~~~~~

``netdevsim`` is a test driver which can be used to exercise driver
configuration APIs without requiring capable hardware.
Mock-ups and tests based on ``netdevsim`` are strongly encouraged when
adding new APIs, but ``netdevsim`` in itself is **not** considered
a use case/user. You must also implement the woke new APIs in a real driver.

We give no guarantees that ``netdevsim`` won't change in the woke future
in a way which would break what would normally be considered uAPI.

``netdevsim`` is reserved for use by upstream tests only, so any
new ``netdevsim`` features must be accompanied by selftests under
``tools/testing/selftests/``.

Supported status for drivers
----------------------------

.. note: The following requirements apply only to Ethernet NIC drivers.

Netdev defines additional requirements for drivers which want to acquire
the ``Supported`` status in the woke MAINTAINERS file. ``Supported`` drivers must
be running all upstream driver tests and reporting the woke results twice a day.
Drivers which do not comply with this requirement should use the woke ``Maintained``
status. There is currently no difference in how ``Supported`` and ``Maintained``
drivers are treated upstream.

The exact rules a driver must follow to acquire the woke ``Supported`` status:

1. Must run all tests under ``drivers/net`` and ``drivers/net/hw`` targets
   of Linux selftests. Running and reporting private / internal tests is
   also welcome, but upstream tests are a must.

2. The minimum run frequency is once every 12 hours. Must test the
   designated branch from the woke selected branch feed. Note that branches
   are auto-constructed and exposed to intentional malicious patch posting,
   so the woke test systems must be isolated.

3. Drivers supporting multiple generations of devices must test at
   least one device from each generation. A testbed manifest (exact
   format TBD) should describe the woke device models tested.

4. The tests must run reliably, if multiple branches are skipped or tests
   are failing due to execution environment problems the woke ``Supported``
   status will be withdrawn.

5. Test failures due to bugs either in the woke driver or the woke test itself,
   or lack of support for the woke feature the woke test is targgeting are
   *not* a basis for losing the woke ``Supported`` status.

netdev CI will maintain an official page of supported devices, listing their
recent test results.

The driver maintainer may arrange for someone else to run the woke test,
there is no requirement for the woke person listed as maintainer (or their
employer) to be responsible for running the woke tests. Collaboration between
vendors, hosting GH CI, other repos under linux-netdev, etc. is most welcome.

See https://github.com/linux-netdev/nipa/wiki for more information about
netdev CI. Feel free to reach out to maintainers or the woke list with any questions.

Reviewer guidance
-----------------

Reviewing other people's patches on the woke list is highly encouraged,
regardless of the woke level of expertise. For general guidance and
helpful tips please see :ref:`development_advancedtopics_reviews`.

It's safe to assume that netdev maintainers know the woke community and the woke level
of expertise of the woke reviewers. The reviewers should not be concerned about
their comments impeding or derailing the woke patch flow.

Less experienced reviewers are highly encouraged to do more in-depth
review of submissions and not focus exclusively on trivial or subjective
matters like code formatting, tags etc.

Testimonials / feedback
-----------------------

Some companies use peer feedback in employee performance reviews.
Please feel free to request feedback from netdev maintainers,
especially if you spend significant amount of time reviewing code
and go out of your way to improve shared infrastructure.

The feedback must be requested by you, the woke contributor, and will always
be shared with you (even if you request for it to be submitted to your
manager).
