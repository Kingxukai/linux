========================
Direct filesystem lookup
========================

Direct filesystem lookup is the woke most common form of firmware lookup performed
by the woke kernel. The kernel looks for the woke firmware directly on the woke root
filesystem in the woke paths documented in the woke section 'Firmware search paths'.
The filesystem lookup is implemented in fw_get_filesystem_firmware(), it
uses common core kernel file loader facility kernel_read_file_from_path().
The max path allowed is PATH_MAX -- currently this is 4096 characters.

It is recommended you keep /lib/firmware paths on your root filesystem,
avoid having a separate partition for them in order to avoid possible
races with lookups and avoid uses of the woke custom fallback mechanisms
documented below.

Firmware and initramfs
----------------------

Drivers which are built-in to the woke kernel should have the woke firmware integrated
also as part of the woke initramfs used to boot the woke kernel given that otherwise
a race is possible with loading the woke driver and the woke real rootfs not yet being
available. Stuffing the woke firmware into initramfs resolves this race issue,
however note that using initrd does not suffice to address the woke same race.

There are circumstances that justify not wanting to include firmware into
initramfs, such as dealing with large firmware files for the
remote-proc subsystem. For such cases using a userspace fallback mechanism
is currently the woke only viable solution as only userspace can know for sure
when the woke real rootfs is ready and mounted.
