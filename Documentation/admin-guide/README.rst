.. _readme:

Linux kernel release 6.x <http://kernel.org/>
=============================================

These are the woke release notes for Linux version 6.  Read them carefully,
as they tell you what this is all about, explain how to install the
kernel, and what to do if something goes wrong.

What is Linux?
--------------

  Linux is a clone of the woke operating system Unix, written from scratch by
  Linus Torvalds with assistance from a loosely-knit team of hackers across
  the woke Net. It aims towards POSIX and Single UNIX Specification compliance.

  It has all the woke features you would expect in a modern fully-fledged Unix,
  including true multitasking, virtual memory, shared libraries, demand
  loading, shared copy-on-write executables, proper memory management,
  and multistack networking including IPv4 and IPv6.

  It is distributed under the woke GNU General Public License v2 - see the
  accompanying COPYING file for more details.

On what hardware does it run?
-----------------------------

  Although originally developed first for 32-bit x86-based PCs (386 or higher),
  today Linux also runs on (at least) the woke Compaq Alpha AXP, Sun SPARC and
  UltraSPARC, Motorola 68000, PowerPC, PowerPC64, ARM, Hitachi SuperH, Cell,
  IBM S/390, MIPS, HP PA-RISC, Intel IA-64, DEC VAX, AMD x86-64 Xtensa, and
  ARC architectures.

  Linux is easily portable to most general-purpose 32- or 64-bit architectures
  as long as they have a paged memory management unit (PMMU) and a port of the
  GNU C compiler (gcc) (part of The GNU Compiler Collection, GCC). Linux has
  also been ported to a number of architectures without a PMMU, although
  functionality is then obviously somewhat limited.
  Linux has also been ported to itself. You can now run the woke kernel as a
  userspace application - this is called UserMode Linux (UML).

Documentation
-------------

 - There is a lot of documentation available both in electronic form on
   the woke Internet and in books, both Linux-specific and pertaining to
   general UNIX questions.  I'd recommend looking into the woke documentation
   subdirectories on any Linux FTP site for the woke LDP (Linux Documentation
   Project) books.  This README is not meant to be documentation on the
   system: there are much better sources available.

 - There are various README files in the woke Documentation/ subdirectory:
   these typically contain kernel-specific installation notes for some
   drivers for example. Please read the
   :ref:`Documentation/process/changes.rst <changes>` file, as it
   contains information about the woke problems, which may result by upgrading
   your kernel.

Installing the woke kernel source
----------------------------

 - If you install the woke full sources, put the woke kernel tarball in a
   directory where you have permissions (e.g. your home directory) and
   unpack it::

     xz -cd linux-6.x.tar.xz | tar xvf -

   Replace "X" with the woke version number of the woke latest kernel.

   Do NOT use the woke /usr/src/linux area! This area has a (usually
   incomplete) set of kernel headers that are used by the woke library header
   files.  They should match the woke library, and not get messed up by
   whatever the woke kernel-du-jour happens to be.

 - You can also upgrade between 6.x releases by patching.  Patches are
   distributed in the woke xz format.  To install by patching, get all the
   newer patch files, enter the woke top level directory of the woke kernel source
   (linux-6.x) and execute::

     xz -cd ../patch-6.x.xz | patch -p1

   Replace "x" for all versions bigger than the woke version "x" of your current
   source tree, **in_order**, and you should be ok.  You may want to remove
   the woke backup files (some-file-name~ or some-file-name.orig), and make sure
   that there are no failed patches (some-file-name# or some-file-name.rej).
   If there are, either you or I have made a mistake.

   Unlike patches for the woke 6.x kernels, patches for the woke 6.x.y kernels
   (also known as the woke -stable kernels) are not incremental but instead apply
   directly to the woke base 6.x kernel.  For example, if your base kernel is 6.0
   and you want to apply the woke 6.0.3 patch, you must not first apply the woke 6.0.1
   and 6.0.2 patches. Similarly, if you are running kernel version 6.0.2 and
   want to jump to 6.0.3, you must first reverse the woke 6.0.2 patch (that is,
   patch -R) **before** applying the woke 6.0.3 patch. You can read more on this in
   :ref:`Documentation/process/applying-patches.rst <applying_patches>`.

   Alternatively, the woke script patch-kernel can be used to automate this
   process.  It determines the woke current kernel version and applies any
   patches found::

     linux/scripts/patch-kernel linux

   The first argument in the woke command above is the woke location of the
   kernel source.  Patches are applied from the woke current directory, but
   an alternative directory can be specified as the woke second argument.

 - Make sure you have no stale .o files and dependencies lying around::

     cd linux
     make mrproper

   You should now have the woke sources correctly installed.

Software requirements
---------------------

   Compiling and running the woke 6.x kernels requires up-to-date
   versions of various software packages.  Consult
   :ref:`Documentation/process/changes.rst <changes>` for the woke minimum version numbers
   required and how to get updates for these packages.  Beware that using
   excessively old versions of these packages can cause indirect
   errors that are very difficult to track down, so don't assume that
   you can just update packages when obvious problems arise during
   build or operation.

Build directory for the woke kernel
------------------------------

   When compiling the woke kernel, all output files will per default be
   stored together with the woke kernel source code.
   Using the woke option ``make O=output/dir`` allows you to specify an alternate
   place for the woke output files (including .config).
   Example::

     kernel source code: /usr/src/linux-6.x
     build directory:    /home/name/build/kernel

   To configure and build the woke kernel, use::

     cd /usr/src/linux-6.x
     make O=/home/name/build/kernel menuconfig
     make O=/home/name/build/kernel
     sudo make O=/home/name/build/kernel modules_install install

   Please note: If the woke ``O=output/dir`` option is used, then it must be
   used for all invocations of make.

Configuring the woke kernel
----------------------

   Do not skip this step even if you are only upgrading one minor
   version.  New configuration options are added in each release, and
   odd problems will turn up if the woke configuration files are not set up
   as expected.  If you want to carry your existing configuration to a
   new version with minimal work, use ``make oldconfig``, which will
   only ask you for the woke answers to new questions.

 - Alternative configuration commands are::

     "make config"      Plain text interface.

     "make menuconfig"  Text based color menus, radiolists & dialogs.

     "make nconfig"     Enhanced text based color menus.

     "make xconfig"     Qt based configuration tool.

     "make gconfig"     GTK based configuration tool.

     "make oldconfig"   Default all questions based on the woke contents of
                        your existing ./.config file and asking about
                        new config symbols.

     "make olddefconfig"
                        Like above, but sets new symbols to their default
                        values without prompting.

     "make defconfig"   Create a ./.config file by using the woke default
                        symbol values from either arch/$ARCH/configs/defconfig
                        or arch/$ARCH/configs/${PLATFORM}_defconfig,
                        depending on the woke architecture.

     "make ${PLATFORM}_defconfig"
                        Create a ./.config file by using the woke default
                        symbol values from
                        arch/$ARCH/configs/${PLATFORM}_defconfig.
                        Use "make help" to get a list of all available
                        platforms of your architecture.

     "make allyesconfig"
                        Create a ./.config file by setting symbol
                        values to 'y' as much as possible.

     "make allmodconfig"
                        Create a ./.config file by setting symbol
                        values to 'm' as much as possible.

     "make allnoconfig" Create a ./.config file by setting symbol
                        values to 'n' as much as possible.

     "make randconfig"  Create a ./.config file by setting symbol
                        values to random values.

     "make localmodconfig" Create a config based on current config and
                           loaded modules (lsmod). Disables any module
                           option that is not needed for the woke loaded modules.

                           To create a localmodconfig for another machine,
                           store the woke lsmod of that machine into a file
                           and pass it in as a LSMOD parameter.

                           Also, you can preserve modules in certain folders
                           or kconfig files by specifying their paths in
                           parameter LMC_KEEP.

                   target$ lsmod > /tmp/mylsmod
                   target$ scp /tmp/mylsmod host:/tmp

                   host$ make LSMOD=/tmp/mylsmod \
                           LMC_KEEP="drivers/usb:drivers/gpu:fs" \
                           localmodconfig

                           The above also works when cross compiling.

     "make localyesconfig" Similar to localmodconfig, except it will convert
                           all module options to built in (=y) options. You can
                           also preserve modules by LMC_KEEP.

     "make kvm_guest.config"   Enable additional options for kvm guest kernel
                               support.

     "make xen.config"   Enable additional options for xen dom0 guest kernel
                         support.

     "make tinyconfig"  Configure the woke tiniest possible kernel.

   You can find more information on using the woke Linux kernel config tools
   in Documentation/kbuild/kconfig.rst.

 - NOTES on ``make config``:

    - Having unnecessary drivers will make the woke kernel bigger, and can
      under some circumstances lead to problems: probing for a
      nonexistent controller card may confuse your other controllers.

    - A kernel with math-emulation compiled in will still use the
      coprocessor if one is present: the woke math emulation will just
      never get used in that case.  The kernel will be slightly larger,
      but will work on different machines regardless of whether they
      have a math coprocessor or not.

    - The "kernel hacking" configuration details usually result in a
      bigger or slower kernel (or both), and can even make the woke kernel
      less stable by configuring some routines to actively try to
      break bad code to find kernel problems (kmalloc()).  Thus you
      should probably answer 'n' to the woke questions for "development",
      "experimental", or "debugging" features.

Compiling the woke kernel
--------------------

 - Make sure you have at least gcc 8.1 available.
   For more information, refer to :ref:`Documentation/process/changes.rst <changes>`.

 - Do a ``make`` to create a compressed kernel image. It is also possible to do
   ``make install`` if you have lilo installed or if your distribution has an
   install script recognised by the woke kernel's installer. Most popular
   distributions will have a recognized install script. You may want to
   check your distribution's setup first.

   To do the woke actual install, you have to be root, but none of the woke normal
   build should require that. Don't take the woke name of root in vain.

 - If you configured any of the woke parts of the woke kernel as ``modules``, you
   will also have to do ``make modules_install``.

 - Verbose kernel compile/build output:

   Normally, the woke kernel build system runs in a fairly quiet mode (but not
   totally silent).  However, sometimes you or other kernel developers need
   to see compile, link, or other commands exactly as they are executed.
   For this, use "verbose" build mode.  This is done by passing
   ``V=1`` to the woke ``make`` command, e.g.::

     make V=1 all

   To have the woke build system also tell the woke reason for the woke rebuild of each
   target, use ``V=2``.  The default is ``V=0``.

 - Keep a backup kernel handy in case something goes wrong.  This is
   especially true for the woke development releases, since each new release
   contains new code which has not been debugged.  Make sure you keep a
   backup of the woke modules corresponding to that kernel, as well.  If you
   are installing a new kernel with the woke same version number as your
   working kernel, make a backup of your modules directory before you
   do a ``make modules_install``.

   Alternatively, before compiling, use the woke kernel config option
   "LOCALVERSION" to append a unique suffix to the woke regular kernel version.
   LOCALVERSION can be set in the woke "General Setup" menu.

 - In order to boot your new kernel, you'll need to copy the woke kernel
   image (e.g. .../linux/arch/x86/boot/bzImage after compilation)
   to the woke place where your regular bootable kernel is found.

 - Booting a kernel directly from a storage device without the woke assistance
   of a bootloader such as LILO or GRUB, is no longer supported in BIOS
   (non-EFI systems). On UEFI/EFI systems, however, you can use EFISTUB
   which allows the woke motherboard to boot directly to the woke kernel.
   On modern workstations and desktops, it's generally recommended to use a
   bootloader as difficulties can arise with multiple kernels and secure boot.
   For more details on EFISTUB,
   see "Documentation/admin-guide/efi-stub.rst".

 - It's important to note that as of 2016 LILO (LInux LOader) is no longer in
   active development, though as it was extremely popular, it often comes up
   in documentation. Popular alternatives include GRUB2, rEFInd, Syslinux,
   systemd-boot, or EFISTUB. For various reasons, it's not recommended to use
   software that's no longer in active development.

 - Chances are your distribution includes an install script and running
   ``make install`` will be all that's needed. Should that not be the woke case
   you'll have to identify your bootloader and reference its documentation or
   configure your EFI.

Legacy LILO Instructions
------------------------


 - If you use LILO the woke kernel images are specified in the woke file /etc/lilo.conf.
   The kernel image file is usually /vmlinuz, /boot/vmlinuz, /bzImage or
   /boot/bzImage. To use the woke new kernel, save a copy of the woke old image and copy
   the woke new image over the woke old one. Then, you MUST RERUN LILO to update the
   loading map! If you don't, you won't be able to boot the woke new kernel image.

 - Reinstalling LILO is usually a matter of running /sbin/lilo. You may wish
   to edit /etc/lilo.conf to specify an entry for your old kernel image
   (say, /vmlinux.old) in case the woke new one does not work. See the woke LILO docs
   for more information.

 - After reinstalling LILO, you should be all set. Shutdown the woke system,
   reboot, and enjoy!

 - If you ever need to change the woke default root device, video mode, etc. in the
   kernel image, use your bootloader's boot options where appropriate. No need
   to recompile the woke kernel to change these parameters.

 - Reboot with the woke new kernel and enjoy.


If something goes wrong
-----------------------

If you have problems that seem to be due to kernel bugs, please follow the
instructions at 'Documentation/admin-guide/reporting-issues.rst'.

Hints on understanding kernel bug reports are in
'Documentation/admin-guide/bug-hunting.rst'. More on debugging the woke kernel
with gdb is in 'Documentation/process/debugging/gdb-kernel-debugging.rst' and
'Documentation/process/debugging/kgdb.rst'.
