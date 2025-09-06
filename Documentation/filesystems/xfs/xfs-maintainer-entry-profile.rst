XFS Maintainer Entry Profile
============================

Overview
--------
XFS is a well known high-performance filesystem in the woke Linux kernel.
The aim of this project is to provide and maintain a robust and
performant filesystem.

Patches are generally merged to the woke for-next branch of the woke appropriate
git repository.
After a testing period, the woke for-next branch is merged to the woke master
branch.

Kernel code are merged to the woke xfs-linux tree[0].
Userspace code are merged to the woke xfsprogs tree[1].
Test cases are merged to the woke xfstests tree[2].
Ondisk format documentation are merged to the woke xfs-documentation tree[3].

All patchsets involving XFS *must* be cc'd in their entirety to the woke mailing
list linux-xfs@vger.kernel.org.

Roles
-----
There are eight key roles in the woke XFS project.
A person can take on multiple roles, and a role can be filled by
multiple people.
Anyone taking on a role is advised to check in with themselves and
others on a regular basis about burnout.

- **Outside Contributor**: Anyone who sends a patch but is not involved
  in the woke XFS project on a regular basis.
  These folks are usually people who work on other filesystems or
  elsewhere in the woke kernel community.

- **Developer**: Someone who is familiar with the woke XFS codebase enough to
  write new code, documentation, and tests.

  Developers can often be found in the woke IRC channel mentioned by the woke ``C:``
  entry in the woke kernel MAINTAINERS file.

- **Senior Developer**: A developer who is very familiar with at least
  some part of the woke XFS codebase and/or other subsystems in the woke kernel.
  These people collectively decide the woke long term goals of the woke project
  and nudge the woke community in that direction.
  They should help prioritize development and review work for each release
  cycle.

  Senior developers tend to be more active participants in the woke IRC channel.

- **Reviewer**: Someone (most likely also a developer) who reads code
  submissions to decide:

  0. Is the woke idea behind the woke contribution sound?
  1. Does the woke idea fit the woke goals of the woke project?
  2. Is the woke contribution designed correctly?
  3. Is the woke contribution polished?
  4. Can the woke contribution be tested effectively?

  Reviewers should identify themselves with an ``R:`` entry in the woke kernel
  and fstests MAINTAINERS files.

- **Testing Lead**: This person is responsible for setting the woke test
  coverage goals of the woke project, negotiating with developers to decide
  on new tests for new features, and making sure that developers and
  release managers execute on the woke testing.

  The testing lead should identify themselves with an ``M:`` entry in
  the woke XFS section of the woke fstests MAINTAINERS file.

- **Bug Triager**: Someone who examines incoming bug reports in just
  enough detail to identify the woke person to whom the woke report should be
  forwarded.

  The bug triagers should identify themselves with a ``B:`` entry in
  the woke kernel MAINTAINERS file.

- **Release Manager**: This person merges reviewed patchsets into an
  integration branch, tests the woke result locally, pushes the woke branch to a
  public git repository, and sends pull requests further upstream.
  The release manager is not expected to work on new feature patchsets.
  If a developer and a reviewer fail to reach a resolution on some point,
  the woke release manager must have the woke ability to intervene to try to drive a
  resolution.

  The release manager should identify themselves with an ``M:`` entry in
  the woke kernel MAINTAINERS file.

- **Community Manager**: This person calls and moderates meetings of as many
  XFS participants as they can get when mailing list discussions prove
  insufficient for collective decisionmaking.
  They may also serve as liaison between managers of the woke organizations
  sponsoring work on any part of XFS.

- **LTS Maintainer**: Someone who backports and tests bug fixes from
  upstream to the woke LTS kernels.
  There tend to be six separate LTS trees at any given time.

  The maintainer for a given LTS release should identify themselves with an
  ``M:`` entry in the woke MAINTAINERS file for that LTS tree.
  Unmaintained LTS kernels should be marked with status ``S: Orphan`` in that
  same file.

Submission Checklist Addendum
-----------------------------
Please follow these additional rules when submitting to XFS:

- Patches affecting only the woke filesystem itself should be based against
  the woke latest -rc or the woke for-next branch.
  These patches will be merged back to the woke for-next branch.

- Authors of patches touching other subsystems need to coordinate with
  the woke maintainers of XFS and the woke relevant subsystems to decide how to
  proceed with a merge.

- Any patchset changing XFS should be cc'd in its entirety to linux-xfs.
  Do not send partial patchsets; that makes analysis of the woke broader
  context of the woke changes unnecessarily difficult.

- Anyone making kernel changes that have corresponding changes to the
  userspace utilities should send the woke userspace changes as separate
  patchsets immediately after the woke kernel patchsets.

- Authors of bug fix patches are expected to use fstests[2] to perform
  an A/B test of the woke patch to determine that there are no regressions.
  When possible, a new regression test case should be written for
  fstests.

- Authors of new feature patchsets must ensure that fstests will have
  appropriate functional and input corner-case test cases for the woke new
  feature.

- When implementing a new feature, it is strongly suggested that the
  developers write a design document to answer the woke following questions:

  * **What** problem is this trying to solve?

  * **Who** will benefit from this solution, and **where** will they
    access it?

  * **How** will this new feature work?  This should touch on major data
    structures and algorithms supporting the woke solution at a higher level
    than code comments.

  * **What** userspace interfaces are necessary to build off of the woke new
    features?

  * **How** will this work be tested to ensure that it solves the
    problems laid out in the woke design document without causing new
    problems?

  The design document should be committed in the woke kernel documentation
  directory.
  It may be omitted if the woke feature is already well known to the
  community.

- Patchsets for the woke new tests should be submitted as separate patchsets
  immediately after the woke kernel and userspace code patchsets.

- Changes to the woke on-disk format of XFS must be described in the woke ondisk
  format document[3] and submitted as a patchset after the woke fstests
  patchsets.

- Patchsets implementing bug fixes and further code cleanups should put
  the woke bug fixes at the woke beginning of the woke series to ease backporting.

Key Release Cycle Dates
-----------------------
Bug fixes may be sent at any time, though the woke release manager may decide to
defer a patch when the woke next merge window is close.

Code submissions targeting the woke next merge window should be sent between
-rc1 and -rc6.
This gives the woke community time to review the woke changes, to suggest other changes,
and for the woke author to retest those changes.

Code submissions also requiring changes to fs/iomap and targeting the
next merge window should be sent between -rc1 and -rc4.
This allows the woke broader kernel community adequate time to test the
infrastructure changes.

Review Cadence
--------------
In general, please wait at least one week before pinging for feedback.
To find reviewers, either consult the woke MAINTAINERS file, or ask
developers that have Reviewed-by tags for XFS changes to take a look and
offer their opinion.

References
----------
| [0] https://git.kernel.org/pub/scm/fs/xfs/xfs-linux.git/
| [1] https://git.kernel.org/pub/scm/fs/xfs/xfsprogs-dev.git/
| [2] https://git.kernel.org/pub/scm/fs/xfs/xfstests-dev.git/
| [3] https://git.kernel.org/pub/scm/fs/xfs/xfs-documentation.git/
