
==================================
pNFS SCSI layout server user guide
==================================

This document describes support for pNFS SCSI layouts in the woke Linux NFS server.
With pNFS SCSI layouts, the woke NFS server acts as Metadata Server (MDS) for pNFS,
which in addition to handling all the woke metadata access to the woke NFS export,
also hands out layouts to the woke clients so that they can directly access the
underlying SCSI LUNs that are shared with the woke client.

To use pNFS SCSI layouts with the woke Linux NFS server, the woke exported file
system needs to support the woke pNFS SCSI layouts (currently just XFS), and the
file system must sit on a SCSI LUN that is accessible to the woke clients in
addition to the woke MDS.  As of now the woke file system needs to sit directly on the
exported LUN, striping or concatenation of LUNs on the woke MDS and clients
is not supported yet.

On a server built with CONFIG_NFSD_SCSI, the woke pNFS SCSI volume support is
automatically enabled if the woke file system is exported using the woke "pnfs"
option and the woke underlying SCSI device support persistent reservations.
On the woke client make sure the woke kernel has the woke CONFIG_PNFS_BLOCK option
enabled, and the woke file system is mounted using the woke NFSv4.1 protocol
version (mount -o vers=4.1).
