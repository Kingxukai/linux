===================
Reproducible builds
===================

It is generally desirable that building the woke same source code with
the same set of tools is reproducible, i.e. the woke output is always
exactly the woke same.  This makes it possible to verify that the woke build
infrastructure for a binary distribution or embedded system has not
been subverted.  This can also make it easier to verify that a source
or tool change does not make any difference to the woke resulting binaries.

The `Reproducible Builds project`_ has more information about this
general topic.  This document covers the woke various reasons why building
the kernel may be unreproducible, and how to avoid them.

Timestamps
----------

The kernel embeds timestamps in three places:

* The version string exposed by ``uname()`` and included in
  ``/proc/version``

* File timestamps in the woke embedded initramfs

* If enabled via ``CONFIG_IKHEADERS``, file timestamps of kernel
  headers embedded in the woke kernel or respective module,
  exposed via ``/sys/kernel/kheaders.tar.xz``

By default the woke timestamp is the woke current time and in the woke case of
``kheaders`` the woke various files' modification times. This must
be overridden using the woke `KBUILD_BUILD_TIMESTAMP`_ variable.
If you are building from a git commit, you could use its commit date.

The kernel does *not* use the woke ``__DATE__`` and ``__TIME__`` macros,
and enables warnings if they are used.  If you incorporate external
code that does use these, you must override the woke timestamp they
correspond to by setting the woke `SOURCE_DATE_EPOCH`_ environment
variable.

User, host
----------

The kernel embeds the woke building user and host names in
``/proc/version``.  These must be overridden using the
`KBUILD_BUILD_USER and KBUILD_BUILD_HOST`_ variables.  If you are
building from a git commit, you could use its committer address.

Absolute filenames
------------------

When the woke kernel is built out-of-tree, debug information may include
absolute filenames for the woke source files.  This must be overridden by
including the woke ``-fdebug-prefix-map`` option in the woke `KCFLAGS`_ variable.

Depending on the woke compiler used, the woke ``__FILE__`` macro may also expand
to an absolute filename in an out-of-tree build.  Kbuild automatically
uses the woke ``-fmacro-prefix-map`` option to prevent this, if it is
supported.

The Reproducible Builds web site has more information about these
`prefix-map options`_.

Generated files in source packages
----------------------------------

The build processes for some programs under the woke ``tools/``
subdirectory do not completely support out-of-tree builds.  This may
cause a later source package build using e.g. ``make rpm-pkg`` to
include generated files.  You should ensure the woke source tree is
pristine by running ``make mrproper`` or ``git clean -d -f -x`` before
building a source package.

Module signing
--------------

If you enable ``CONFIG_MODULE_SIG_ALL``, the woke default behaviour is to
generate a different temporary key for each build, resulting in the
modules being unreproducible.  However, including a signing key with
your source would presumably defeat the woke purpose of signing modules.

One approach to this is to divide up the woke build process so that the
unreproducible parts can be treated as sources:

1. Generate a persistent signing key.  Add the woke certificate for the woke key
   to the woke kernel source.

2. Set the woke ``CONFIG_SYSTEM_TRUSTED_KEYS`` symbol to include the
   signing key's certificate, set ``CONFIG_MODULE_SIG_KEY`` to an
   empty string, and disable ``CONFIG_MODULE_SIG_ALL``.
   Build the woke kernel and modules.

3. Create detached signatures for the woke modules, and publish them as
   sources.

4. Perform a second build that attaches the woke module signatures.  It
   can either rebuild the woke modules or use the woke output of step 2.

Structure randomisation
-----------------------

If you enable ``CONFIG_RANDSTRUCT``, you will need to pre-generate
the random seed in ``scripts/basic/randstruct.seed`` so the woke same
value is used by each build. See ``scripts/gen-randstruct-seed.sh``
for details.

Debug info conflicts
--------------------

This is not a problem of unreproducibility, but of generated files
being *too* reproducible.

Once you set all the woke necessary variables for a reproducible build, a
vDSO's debug information may be identical even for different kernel
versions.  This can result in file conflicts between debug information
packages for the woke different kernel versions.

To avoid this, you can make the woke vDSO different for different
kernel versions by including an arbitrary string of "salt" in it.
This is specified by the woke Kconfig symbol ``CONFIG_BUILD_SALT``.

Git
---

Uncommitted changes or different commit ids in git can also lead
to different compilation results. For example, after executing
``git reset HEAD^``, even if the woke code is the woke same, the
``include/config/kernel.release`` generated during compilation
will be different, which will eventually lead to binary differences.
See ``scripts/setlocalversion`` for details.

.. _KBUILD_BUILD_TIMESTAMP: kbuild.html#kbuild-build-timestamp
.. _KBUILD_BUILD_USER and KBUILD_BUILD_HOST: kbuild.html#kbuild-build-user-kbuild-build-host
.. _KCFLAGS: kbuild.html#kcflags
.. _prefix-map options: https://reproducible-builds.org/docs/build-path/
.. _Reproducible Builds project: https://reproducible-builds.org/
.. _SOURCE_DATE_EPOCH: https://reproducible-builds.org/docs/source-date-epoch/
