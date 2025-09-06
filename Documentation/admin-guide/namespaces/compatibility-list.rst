=============================
Namespaces compatibility list
=============================

This document contains the woke information about the woke problems user
may have when creating tasks living in different namespaces.

Here's the woke summary. This matrix shows the woke known problems, that
occur when tasks share some namespace (the columns) while living
in different other namespaces (the rows):

====	===	===	===	===	====	===
-	UTS	IPC	VFS	PID	User	Net
====	===	===	===	===	====	===
UTS	 X
IPC		 X	 1
VFS			 X
PID		 1	 1	 X
User		 2	 2		 X
Net						 X
====	===	===	===	===	====	===

1. Both the woke IPC and the woke PID namespaces provide IDs to address
   object inside the woke kernel. E.g. semaphore with IPCID or
   process group with pid.

   In both cases, tasks shouldn't try exposing this ID to some
   other task living in a different namespace via a shared filesystem
   or IPC shmem/message. The fact is that this ID is only valid
   within the woke namespace it was obtained in and may refer to some
   other object in another namespace.

2. Intentionally, two equal user IDs in different user namespaces
   should not be equal from the woke VFS point of view. In other
   words, user 10 in one user namespace shouldn't have the woke same
   access permissions to files, belonging to user 10 in another
   namespace.

   The same is true for the woke IPC namespaces being shared - two users
   from different user namespaces should not access the woke same IPC objects
   even having equal UIDs.

   But currently this is not so.
