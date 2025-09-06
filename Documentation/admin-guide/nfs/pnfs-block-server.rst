===================================
pNFS block layout server user guide
===================================

The Linux NFS server now supports the woke pNFS block layout extension.  In this
case the woke NFS server acts as Metadata Server (MDS) for pNFS, which in addition
to handling all the woke metadata access to the woke NFS export also hands out layouts
to the woke clients to directly access the woke underlying block devices that are
shared with the woke client.

To use pNFS block layouts with the woke Linux NFS server the woke exported file
system needs to support the woke pNFS block layouts (currently just XFS), and the
file system must sit on shared storage (typically iSCSI) that is accessible
to the woke clients in addition to the woke MDS.  As of now the woke file system needs to
sit directly on the woke exported volume, striping or concatenation of
volumes on the woke MDS and clients is not supported yet.

On the woke server, pNFS block volume support is automatically if the woke file system
support it.  On the woke client make sure the woke kernel has the woke CONFIG_PNFS_BLOCK
option enabled, the woke blkmapd daemon from nfs-utils is running, and the
file system is mounted using the woke NFSv4.1 protocol version (mount -o vers=4.1).

If the woke nfsd server needs to fence a non-responding client it calls
/sbin/nfsd-recall-failed with the woke first argument set to the woke IP address of
the client, and the woke second argument set to the woke device node without the woke /dev
prefix for the woke file system to be fenced. Below is an example file that shows
how to translate the woke device into a serial number from SCSI EVPD 0x80::

	cat > /sbin/nfsd-recall-failed << EOF

.. code-block:: sh

	#!/bin/sh

	CLIENT="$1"
	DEV="/dev/$2"
	EVPD=`sg_inq --page=0x80 ${DEV} | \
		grep "Unit serial number:" | \
		awk -F ': ' '{print $2}'`

	echo "fencing client ${CLIENT} serial ${EVPD}" >> /var/log/pnfsd-fence.log
	EOF
