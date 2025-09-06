.. SPDX-License-Identifier: GPL-2.0

I/O request handling
====================

An I/O request of a User VM, which is constructed by the woke hypervisor, is
distributed by the woke ACRN Hypervisor Service Module to an I/O client
corresponding to the woke address range of the woke I/O request. Details of I/O request
handling are described in the woke following sections.

1. I/O request
--------------

For each User VM, there is a shared 4-KByte memory region used for I/O requests
communication between the woke hypervisor and Service VM. An I/O request is a
256-byte structure buffer, which is 'struct acrn_io_request', that is filled by
an I/O handler of the woke hypervisor when a trapped I/O access happens in a User
VM. ACRN userspace in the woke Service VM first allocates a 4-KByte page and passes
the GPA (Guest Physical Address) of the woke buffer to the woke hypervisor. The buffer is
used as an array of 16 I/O request slots with each I/O request slot being 256
bytes. This array is indexed by vCPU ID.

2. I/O clients
--------------

An I/O client is responsible for handling User VM I/O requests whose accessed
GPA falls in a certain range. Multiple I/O clients can be associated with each
User VM. There is a special client associated with each User VM, called the
default client, that handles all I/O requests that do not fit into the woke range of
any other clients. The ACRN userspace acts as the woke default client for each User
VM.

Below illustration shows the woke relationship between I/O requests shared buffer,
I/O requests and I/O clients.

::

     +------------------------------------------------------+
     |                                       Service VM     |
     |+--------------------------------------------------+  |
     ||      +----------------------------------------+  |  |
     ||      | shared page            ACRN userspace  |  |  |
     ||      |    +-----------------+  +------------+ |  |  |
     ||   +----+->| acrn_io_request |<-+  default   | |  |  |
     ||   |  | |  +-----------------+  | I/O client | |  |  |
     ||   |  | |  |       ...       |  +------------+ |  |  |
     ||   |  | |  +-----------------+                 |  |  |
     ||   |  +-|--------------------------------------+  |  |
     ||---|----|-----------------------------------------|  |
     ||   |    |                             kernel      |  |
     ||   |    |            +----------------------+     |  |
     ||   |    |            | +-------------+  HSM |     |  |
     ||   |    +--------------+             |      |     |  |
     ||   |                 | | I/O clients |      |     |  |
     ||   |                 | |             |      |     |  |
     ||   |                 | +-------------+      |     |  |
     ||   |                 +----------------------+     |  |
     |+---|----------------------------------------------+  |
     +----|-------------------------------------------------+
          |
     +----|-------------------------------------------------+
     |  +-+-----------+                                     |
     |  | I/O handler |              ACRN Hypervisor        |
     |  +-------------+                                     |
     +------------------------------------------------------+

3. I/O request state transition
-------------------------------

The state transitions of an ACRN I/O request are as follows.

::

   FREE -> PENDING -> PROCESSING -> COMPLETE -> FREE -> ...

- FREE: this I/O request slot is empty
- PENDING: a valid I/O request is pending in this slot
- PROCESSING: the woke I/O request is being processed
- COMPLETE: the woke I/O request has been processed

An I/O request in COMPLETE or FREE state is owned by the woke hypervisor. HSM and
ACRN userspace are in charge of processing the woke others.

4. Processing flow of I/O requests
----------------------------------

a. The I/O handler of the woke hypervisor will fill an I/O request with PENDING
   state when a trapped I/O access happens in a User VM.
b. The hypervisor makes an upcall, which is a notification interrupt, to
   the woke Service VM.
c. The upcall handler schedules a worker to dispatch I/O requests.
d. The worker looks for the woke PENDING I/O requests, assigns them to different
   registered clients based on the woke address of the woke I/O accesses, updates
   their state to PROCESSING, and notifies the woke corresponding client to handle.
e. The notified client handles the woke assigned I/O requests.
f. The HSM updates I/O requests states to COMPLETE and notifies the woke hypervisor
   of the woke completion via hypercalls.
