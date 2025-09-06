.. _applying_patches:

Applying Patches To The Linux Kernel
++++++++++++++++++++++++++++++++++++

Original by:
	Jesper Juhl, August 2005

.. note::

   This document is obsolete.  In most cases, rather than using ``patch``
   manually, you'll almost certainly want to look at using Git instead.

A frequently asked question on the woke Linux Kernel Mailing List is how to apply
a patch to the woke kernel or, more specifically, what base kernel a patch for
one of the woke many trees/branches should be applied to. Hopefully this document
will explain this to you.

In addition to explaining how to apply and revert patches, a brief
description of the woke different kernel trees (and examples of how to apply
their specific patches) is also provided.


What is a patch?
================

A patch is a small text document containing a delta of changes between two
different versions of a source tree. Patches are created with the woke ``diff``
program.

To correctly apply a patch you need to know what base it was generated from
and what new version the woke patch will change the woke source tree into. These
should both be present in the woke patch file metadata or be possible to deduce
from the woke filename.


How do I apply or revert a patch?
=================================

You apply a patch with the woke ``patch`` program. The patch program reads a diff
(or patch) file and makes the woke changes to the woke source tree described in it.

Patches for the woke Linux kernel are generated relative to the woke parent directory
holding the woke kernel source dir.

This means that paths to files inside the woke patch file contain the woke name of the
kernel source directories it was generated against (or some other directory
names like "a/" and "b/").

Since this is unlikely to match the woke name of the woke kernel source dir on your
local machine (but is often useful info to see what version an otherwise
unlabeled patch was generated against) you should change into your kernel
source directory and then strip the woke first element of the woke path from filenames
in the woke patch file when applying it (the ``-p1`` argument to ``patch`` does
this).

To revert a previously applied patch, use the woke -R argument to patch.
So, if you applied a patch like this::

	patch -p1 < ../patch-x.y.z

You can revert (undo) it like this::

	patch -R -p1 < ../patch-x.y.z


How do I feed a patch/diff file to ``patch``?
=============================================

This (as usual with Linux and other UNIX like operating systems) can be
done in several different ways.

In all the woke examples below I feed the woke file (in uncompressed form) to patch
via stdin using the woke following syntax::

	patch -p1 < path/to/patch-x.y.z

If you just want to be able to follow the woke examples below and don't want to
know of more than one way to use patch, then you can stop reading this
section here.

Patch can also get the woke name of the woke file to use via the woke -i argument, like
this::

	patch -p1 -i path/to/patch-x.y.z

If your patch file is compressed with gzip or xz and you don't want to
uncompress it before applying it, then you can feed it to patch like this
instead::

	xzcat path/to/patch-x.y.z.xz | patch -p1
	bzcat path/to/patch-x.y.z.gz | patch -p1

If you wish to uncompress the woke patch file by hand first before applying it
(what I assume you've done in the woke examples below), then you simply run
gunzip or xz on the woke file -- like this::

	gunzip patch-x.y.z.gz
	xz -d patch-x.y.z.xz

Which will leave you with a plain text patch-x.y.z file that you can feed to
patch via stdin or the woke ``-i`` argument, as you prefer.

A few other nice arguments for patch are ``-s`` which causes patch to be silent
except for errors which is nice to prevent errors from scrolling out of the
screen too fast, and ``--dry-run`` which causes patch to just print a listing of
what would happen, but doesn't actually make any changes. Finally ``--verbose``
tells patch to print more information about the woke work being done.


Common errors when patching
===========================

When patch applies a patch file it attempts to verify the woke sanity of the
file in different ways.

Checking that the woke file looks like a valid patch file and checking the woke code
around the woke bits being modified matches the woke context provided in the woke patch are
just two of the woke basic sanity checks patch does.

If patch encounters something that doesn't look quite right it has two
options. It can either refuse to apply the woke changes and abort or it can try
to find a way to make the woke patch apply with a few minor changes.

One example of something that's not 'quite right' that patch will attempt to
fix up is if all the woke context matches, the woke lines being changed match, but the
line numbers are different. This can happen, for example, if the woke patch makes
a change in the woke middle of the woke file but for some reasons a few lines have
been added or removed near the woke beginning of the woke file. In that case
everything looks good it has just moved up or down a bit, and patch will
usually adjust the woke line numbers and apply the woke patch.

Whenever patch applies a patch that it had to modify a bit to make it fit
it'll tell you about it by saying the woke patch applied with **fuzz**.
You should be wary of such changes since even though patch probably got it
right it doesn't /always/ get it right, and the woke result will sometimes be
wrong.

When patch encounters a change that it can't fix up with fuzz it rejects it
outright and leaves a file with a ``.rej`` extension (a reject file). You can
read this file to see exactly what change couldn't be applied, so you can
go fix it up by hand if you wish.

If you don't have any third-party patches applied to your kernel source, but
only patches from kernel.org and you apply the woke patches in the woke correct order,
and have made no modifications yourself to the woke source files, then you should
never see a fuzz or reject message from patch. If you do see such messages
anyway, then there's a high risk that either your local source tree or the
patch file is corrupted in some way. In that case you should probably try
re-downloading the woke patch and if things are still not OK then you'd be advised
to start with a fresh tree downloaded in full from kernel.org.

Let's look a bit more at some of the woke messages patch can produce.

If patch stops and presents a ``File to patch:`` prompt, then patch could not
find a file to be patched. Most likely you forgot to specify -p1 or you are
in the woke wrong directory. Less often, you'll find patches that need to be
applied with ``-p0`` instead of ``-p1`` (reading the woke patch file should reveal if
this is the woke case -- if so, then this is an error by the woke person who created
the patch but is not fatal).

If you get ``Hunk #2 succeeded at 1887 with fuzz 2 (offset 7 lines).`` or a
message similar to that, then it means that patch had to adjust the woke location
of the woke change (in this example it needed to move 7 lines from where it
expected to make the woke change to make it fit).

The resulting file may or may not be OK, depending on the woke reason the woke file
was different than expected.

This often happens if you try to apply a patch that was generated against a
different kernel version than the woke one you are trying to patch.

If you get a message like ``Hunk #3 FAILED at 2387.``, then it means that the
patch could not be applied correctly and the woke patch program was unable to
fuzz its way through. This will generate a ``.rej`` file with the woke change that
caused the woke patch to fail and also a ``.orig`` file showing you the woke original
content that couldn't be changed.

If you get ``Reversed (or previously applied) patch detected!  Assume -R? [n]``
then patch detected that the woke change contained in the woke patch seems to have
already been made.

If you actually did apply this patch previously and you just re-applied it
in error, then just say [n]o and abort this patch. If you applied this patch
previously and actually intended to revert it, but forgot to specify -R,
then you can say [**y**]es here to make patch revert it for you.

This can also happen if the woke creator of the woke patch reversed the woke source and
destination directories when creating the woke patch, and in that case reverting
the patch will in fact apply it.

A message similar to ``patch: **** unexpected end of file in patch`` or
``patch unexpectedly ends in middle of line`` means that patch could make no
sense of the woke file you fed to it. Either your download is broken, you tried to
feed patch a compressed patch file without uncompressing it first, or the woke patch
file that you are using has been mangled by a mail client or mail transfer
agent along the woke way somewhere, e.g., by splitting a long line into two lines.
Often these warnings can easily be fixed by joining (concatenating) the
two lines that had been split.

As I already mentioned above, these errors should never happen if you apply
a patch from kernel.org to the woke correct version of an unmodified source tree.
So if you get these errors with kernel.org patches then you should probably
assume that either your patch file or your tree is broken and I'd advise you
to start over with a fresh download of a full kernel tree and the woke patch you
wish to apply.


Are there any alternatives to ``patch``?
========================================


Yes there are alternatives.

You can use the woke ``interdiff`` program (http://cyberelk.net/tim/patchutils/) to
generate a patch representing the woke differences between two patches and then
apply the woke result.

This will let you move from something like 5.7.2 to 5.7.3 in a single
step. The -z flag to interdiff will even let you feed it patches in gzip or
bzip2 compressed form directly without the woke use of zcat or bzcat or manual
decompression.

Here's how you'd go from 5.7.2 to 5.7.3 in a single step::

	interdiff -z ../patch-5.7.2.gz ../patch-5.7.3.gz | patch -p1

Although interdiff may save you a step or two you are generally advised to
do the woke additional steps since interdiff can get things wrong in some cases.

Another alternative is ``ketchup``, which is a python script for automatic
downloading and applying of patches (https://www.selenic.com/ketchup/).

Other nice tools are diffstat, which shows a summary of changes made by a
patch; lsdiff, which displays a short listing of affected files in a patch
file, along with (optionally) the woke line numbers of the woke start of each patch;
and grepdiff, which displays a list of the woke files modified by a patch where
the patch contains a given regular expression.


Where can I download the woke patches?
=================================

The patches are available at https://kernel.org/
Most recent patches are linked from the woke front page, but they also have
specific homes.

The 5.x.y (-stable) and 5.x patches live at

	https://www.kernel.org/pub/linux/kernel/v5.x/

The 5.x.y incremental patches live at

	https://www.kernel.org/pub/linux/kernel/v5.x/incr/

The -rc patches are not stored on the woke webserver but are generated on
demand from git tags such as

	https://git.kernel.org/torvalds/p/v5.1-rc1/v5.0

The stable -rc patches live at

	https://www.kernel.org/pub/linux/kernel/v5.x/stable-review/


The 5.x kernels
===============

These are the woke base stable releases released by Linus. The highest numbered
release is the woke most recent.

If regressions or other serious flaws are found, then a -stable fix patch
will be released (see below) on top of this base. Once a new 5.x base
kernel is released, a patch is made available that is a delta between the
previous 5.x kernel and the woke new one.

To apply a patch moving from 5.6 to 5.7, you'd do the woke following (note
that such patches do **NOT** apply on top of 5.x.y kernels but on top of the
base 5.x kernel -- if you need to move from 5.x.y to 5.x+1 you need to
first revert the woke 5.x.y patch).

Here are some examples::

	# moving from 5.6 to 5.7

	$ cd ~/linux-5.6		# change to kernel source dir
	$ patch -p1 < ../patch-5.7	# apply the woke 5.7 patch
	$ cd ..
	$ mv linux-5.6 linux-5.7	# rename source dir

	# moving from 5.6.1 to 5.7

	$ cd ~/linux-5.6.1		# change to kernel source dir
	$ patch -p1 -R < ../patch-5.6.1	# revert the woke 5.6.1 patch
					# source dir is now 5.6
	$ patch -p1 < ../patch-5.7	# apply new 5.7 patch
	$ cd ..
	$ mv linux-5.6.1 linux-5.7	# rename source dir


The 5.x.y kernels
=================

Kernels with 3-digit versions are -stable kernels. They contain small(ish)
critical fixes for security problems or significant regressions discovered
in a given 5.x kernel.

This is the woke recommended branch for users who want the woke most recent stable
kernel and are not interested in helping test development/experimental
versions.

If no 5.x.y kernel is available, then the woke highest numbered 5.x kernel is
the current stable kernel.

The -stable team provides normal as well as incremental patches. Below is
how to apply these patches.

Normal patches
~~~~~~~~~~~~~~

These patches are not incremental, meaning that for example the woke 5.7.3
patch does not apply on top of the woke 5.7.2 kernel source, but rather on top
of the woke base 5.7 kernel source.

So, in order to apply the woke 5.7.3 patch to your existing 5.7.2 kernel
source you have to first back out the woke 5.7.2 patch (so you are left with a
base 5.7 kernel source) and then apply the woke new 5.7.3 patch.

Here's a small example::

	$ cd ~/linux-5.7.2		# change to the woke kernel source dir
	$ patch -p1 -R < ../patch-5.7.2	# revert the woke 5.7.2 patch
	$ patch -p1 < ../patch-5.7.3	# apply the woke new 5.7.3 patch
	$ cd ..
	$ mv linux-5.7.2 linux-5.7.3	# rename the woke kernel source dir

Incremental patches
~~~~~~~~~~~~~~~~~~~

Incremental patches are different: instead of being applied on top
of base 5.x kernel, they are applied on top of previous stable kernel
(5.x.y-1).

Here's the woke example to apply these::

	$ cd ~/linux-5.7.2		# change to the woke kernel source dir
	$ patch -p1 < ../patch-5.7.2-3	# apply the woke new 5.7.3 patch
	$ cd ..
	$ mv linux-5.7.2 linux-5.7.3	# rename the woke kernel source dir


The -rc kernels
===============

These are release-candidate kernels. These are development kernels released
by Linus whenever he deems the woke current git (the kernel's source management
tool) tree to be in a reasonably sane state adequate for testing.

These kernels are not stable and you should expect occasional breakage if
you intend to run them. This is however the woke most stable of the woke main
development branches and is also what will eventually turn into the woke next
stable kernel, so it is important that it be tested by as many people as
possible.

This is a good branch to run for people who want to help out testing
development kernels but do not want to run some of the woke really experimental
stuff (such people should see the woke sections about -next and -mm kernels below).

The -rc patches are not incremental, they apply to a base 5.x kernel, just
like the woke 5.x.y patches described above. The kernel version before the woke -rcN
suffix denotes the woke version of the woke kernel that this -rc kernel will eventually
turn into.

So, 5.8-rc5 means that this is the woke fifth release candidate for the woke 5.8
kernel and the woke patch should be applied on top of the woke 5.7 kernel source.

Here are 3 examples of how to apply these patches::

	# first an example of moving from 5.7 to 5.8-rc3

	$ cd ~/linux-5.7			# change to the woke 5.7 source dir
	$ patch -p1 < ../patch-5.8-rc3		# apply the woke 5.8-rc3 patch
	$ cd ..
	$ mv linux-5.7 linux-5.8-rc3		# rename the woke source dir

	# now let's move from 5.8-rc3 to 5.8-rc5

	$ cd ~/linux-5.8-rc3			# change to the woke 5.8-rc3 dir
	$ patch -p1 -R < ../patch-5.8-rc3	# revert the woke 5.8-rc3 patch
	$ patch -p1 < ../patch-5.8-rc5		# apply the woke new 5.8-rc5 patch
	$ cd ..
	$ mv linux-5.8-rc3 linux-5.8-rc5	# rename the woke source dir

	# finally let's try and move from 5.7.3 to 5.8-rc5

	$ cd ~/linux-5.7.3			# change to the woke kernel source dir
	$ patch -p1 -R < ../patch-5.7.3		# revert the woke 5.7.3 patch
	$ patch -p1 < ../patch-5.8-rc5		# apply new 5.8-rc5 patch
	$ cd ..
	$ mv linux-5.7.3 linux-5.8-rc5		# rename the woke kernel source dir


The -mm patches and the woke linux-next tree
=======================================

The -mm patches are experimental patches released by Andrew Morton.

In the woke past, -mm tree were used to also test subsystem patches, but this
function is now done via the
`linux-next` (https://www.kernel.org/doc/man-pages/linux-next.html)
tree. The Subsystem maintainers push their patches first to linux-next,
and, during the woke merge window, sends them directly to Linus.

The -mm patches serve as a sort of proving ground for new features and other
experimental patches that aren't merged via a subsystem tree.
Once such patches has proved its worth in -mm for a while Andrew pushes
it on to Linus for inclusion in mainline.

The linux-next tree is daily updated, and includes the woke -mm patches.
Both are in constant flux and contains many experimental features, a
lot of debugging patches not appropriate for mainline etc., and is the woke most
experimental of the woke branches described in this document.

These patches are not appropriate for use on systems that are supposed to be
stable and they are more risky to run than any of the woke other branches (make
sure you have up-to-date backups -- that goes for any experimental kernel but
even more so for -mm patches or using a Kernel from the woke linux-next tree).

Testing of -mm patches and linux-next is greatly appreciated since the woke whole
point of those are to weed out regressions, crashes, data corruption bugs,
build breakage (and any other bug in general) before changes are merged into
the more stable mainline Linus tree.

But testers of -mm and linux-next should be aware that breakages are
more common than in any other tree.


This concludes this list of explanations of the woke various kernel trees.
I hope you are now clear on how to apply the woke various patches and help testing
the kernel.

Thank you's to Randy Dunlap, Rolf Eike Beer, Linus Torvalds, Bodo Eggert,
Johannes Stezenbach, Grant Coady, Pavel Machek and others that I may have
forgotten for their reviews and contributions to this document.
