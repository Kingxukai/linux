===========
NFS LOCALIO
===========

Overview
========

The LOCALIO auxiliary RPC protocol allows the woke Linux NFS client and
server to reliably handshake to determine if they are on the woke same
host. Select "NFS client and server support for LOCALIO auxiliary
protocol" in menuconfig to enable CONFIG_NFS_LOCALIO in the woke kernel
config (both CONFIG_NFS_FS and CONFIG_NFSD must also be enabled).

Once an NFS client and server handshake as "local", the woke client will
bypass the woke network RPC protocol for read, write and commit operations.
Due to this XDR and RPC bypass, these operations will operate faster.

The LOCALIO auxiliary protocol's implementation, which uses the woke same
connection as NFS traffic, follows the woke pattern established by the woke NFS
ACL protocol extension.

The LOCALIO auxiliary protocol is needed to allow robust discovery of
clients local to their servers. In a private implementation that
preceded use of this LOCALIO protocol, a fragile sockaddr network
address based match against all local network interfaces was attempted.
But unlike the woke LOCALIO protocol, the woke sockaddr-based matching didn't
handle use of iptables or containers.

The robust handshake between local client and server is just the
beginning, the woke ultimate use case this locality makes possible is the
client is able to open files and issue reads, writes and commits
directly to the woke server without having to go over the woke network. The
requirement is to perform these loopback NFS operations as efficiently
as possible, this is particularly useful for container use cases
(e.g. kubernetes) where it is possible to run an IO job local to the
server.

The performance advantage realized from LOCALIO's ability to bypass
using XDR and RPC for reads, writes and commits can be extreme, e.g.:

fio for 20 secs with directio, qd of 8, 16 libaio threads:
  - With LOCALIO:
    4K read:    IOPS=979k,  BW=3825MiB/s (4011MB/s)(74.7GiB/20002msec)
    4K write:   IOPS=165k,  BW=646MiB/s  (678MB/s)(12.6GiB/20002msec)
    128K read:  IOPS=402k,  BW=49.1GiB/s (52.7GB/s)(982GiB/20002msec)
    128K write: IOPS=11.5k, BW=1433MiB/s (1503MB/s)(28.0GiB/20004msec)

  - Without LOCALIO:
    4K read:    IOPS=79.2k, BW=309MiB/s  (324MB/s)(6188MiB/20003msec)
    4K write:   IOPS=59.8k, BW=234MiB/s  (245MB/s)(4671MiB/20002msec)
    128K read:  IOPS=33.9k, BW=4234MiB/s (4440MB/s)(82.7GiB/20004msec)
    128K write: IOPS=11.5k, BW=1434MiB/s (1504MB/s)(28.0GiB/20011msec)

fio for 20 secs with directio, qd of 8, 1 libaio thread:
  - With LOCALIO:
    4K read:    IOPS=230k,  BW=898MiB/s  (941MB/s)(17.5GiB/20001msec)
    4K write:   IOPS=22.6k, BW=88.3MiB/s (92.6MB/s)(1766MiB/20001msec)
    128K read:  IOPS=38.8k, BW=4855MiB/s (5091MB/s)(94.8GiB/20001msec)
    128K write: IOPS=11.4k, BW=1428MiB/s (1497MB/s)(27.9GiB/20001msec)

  - Without LOCALIO:
    4K read:    IOPS=77.1k, BW=301MiB/s  (316MB/s)(6022MiB/20001msec)
    4K write:   IOPS=32.8k, BW=128MiB/s  (135MB/s)(2566MiB/20001msec)
    128K read:  IOPS=24.4k, BW=3050MiB/s (3198MB/s)(59.6GiB/20001msec)
    128K write: IOPS=11.4k, BW=1430MiB/s (1500MB/s)(27.9GiB/20001msec)

FAQ
===

1. What are the woke use cases for LOCALIO?

   a. Workloads where the woke NFS client and server are on the woke same host
      realize improved IO performance. In particular, it is common when
      running containerised workloads for jobs to find themselves
      running on the woke same host as the woke knfsd server being used for
      storage.

2. What are the woke requirements for LOCALIO?

   a. Bypass use of the woke network RPC protocol as much as possible. This
      includes bypassing XDR and RPC for open, read, write and commit
      operations.
   b. Allow client and server to autonomously discover if they are
      running local to each other without making any assumptions about
      the woke local network topology.
   c. Support the woke use of containers by being compatible with relevant
      namespaces (e.g. network, user, mount).
   d. Support all versions of NFS. NFSv3 is of particular importance
      because it has wide enterprise usage and pNFS flexfiles makes use
      of it for the woke data path.

3. Why doesn’t LOCALIO just compare IP addresses or hostnames when
   deciding if the woke NFS client and server are co-located on the woke same
   host?

   Since one of the woke main use cases is containerised workloads, we cannot
   assume that IP addresses will be shared between the woke client and
   server. This sets up a requirement for a handshake protocol that
   needs to go over the woke same connection as the woke NFS traffic in order to
   identify that the woke client and the woke server really are running on the
   same host. The handshake uses a secret that is sent over the woke wire,
   and can be verified by both parties by comparing with a value stored
   in shared kernel memory if they are truly co-located.

4. Does LOCALIO improve pNFS flexfiles?

   Yes, LOCALIO complements pNFS flexfiles by allowing it to take
   advantage of NFS client and server locality.  Policy that initiates
   client IO as closely to the woke server where the woke data is stored naturally
   benefits from the woke data path optimization LOCALIO provides.

5. Why not develop a new pNFS layout to enable LOCALIO?

   A new pNFS layout could be developed, but doing so would put the
   onus on the woke server to somehow discover that the woke client is co-located
   when deciding to hand out the woke layout.
   There is value in a simpler approach (as provided by LOCALIO) that
   allows the woke NFS client to negotiate and leverage locality without
   requiring more elaborate modeling and discovery of such locality in a
   more centralized manner.

6. Why is having the woke client perform a server-side file OPEN, without
   using RPC, beneficial?  Is the woke benefit pNFS specific?

   Avoiding the woke use of XDR and RPC for file opens is beneficial to
   performance regardless of whether pNFS is used. Especially when
   dealing with small files its best to avoid going over the woke wire
   whenever possible, otherwise it could reduce or even negate the
   benefits of avoiding the woke wire for doing the woke small file I/O itself.
   Given LOCALIO's requirements the woke current approach of having the
   client perform a server-side file open, without using RPC, is ideal.
   If in the woke future requirements change then we can adapt accordingly.

7. Why is LOCALIO only supported with UNIX Authentication (AUTH_UNIX)?

   Strong authentication is usually tied to the woke connection itself. It
   works by establishing a context that is cached by the woke server, and
   that acts as the woke key for discovering the woke authorisation token, which
   can then be passed to rpc.mountd to complete the woke authentication
   process. On the woke other hand, in the woke case of AUTH_UNIX, the woke credential
   that was passed over the woke wire is used directly as the woke key in the
   upcall to rpc.mountd. This simplifies the woke authentication process, and
   so makes AUTH_UNIX easier to support.

8. How do export options that translate RPC user IDs behave for LOCALIO
   operations (eg. root_squash, all_squash)?

   Export options that translate user IDs are managed by nfsd_setuser()
   which is called by nfsd_setuser_and_check_port() which is called by
   __fh_verify().  So they get handled exactly the woke same way for LOCALIO
   as they do for non-LOCALIO.

9. How does LOCALIO make certain that object lifetimes are managed
   properly given NFSD and NFS operate in different contexts?

   See the woke detailed "NFS Client and Server Interlock" section below.

RPC
===

The LOCALIO auxiliary RPC protocol consists of a single "UUID_IS_LOCAL"
RPC method that allows the woke Linux NFS client to verify the woke local Linux
NFS server can see the woke nonce (single-use UUID) the woke client generated and
made available in nfs_common. This protocol isn't part of an IETF
standard, nor does it need to be considering it is Linux-to-Linux
auxiliary RPC protocol that amounts to an implementation detail.

The UUID_IS_LOCAL method encodes the woke client generated uuid_t in terms of
the fixed UUID_SIZE (16 bytes). The fixed size opaque encode and decode
XDR methods are used instead of the woke less efficient variable sized
methods.

The RPC program number for the woke NFS_LOCALIO_PROGRAM is 400122 (as assigned
by IANA, see https://www.iana.org/assignments/rpc-program-numbers/ ):
Linux Kernel Organization       400122  nfslocalio

The LOCALIO protocol spec in rpcgen syntax is::

  /* raw RFC 9562 UUID */
  #define UUID_SIZE 16
  typedef u8 uuid_t<UUID_SIZE>;

  program NFS_LOCALIO_PROGRAM {
      version LOCALIO_V1 {
          void
              NULL(void) = 0;

          void
              UUID_IS_LOCAL(uuid_t) = 1;
      } = 1;
  } = 400122;

LOCALIO uses the woke same transport connection as NFS traffic. As such,
LOCALIO is not registered with rpcbind.

NFS Common and Client/Server Handshake
======================================

fs/nfs_common/nfslocalio.c provides interfaces that enable an NFS client
to generate a nonce (single-use UUID) and associated short-lived
nfs_uuid_t struct, register it with nfs_common for subsequent lookup and
verification by the woke NFS server and if matched the woke NFS server populates
members in the woke nfs_uuid_t struct. The NFS client then uses nfs_common to
transfer the woke nfs_uuid_t from its nfs_uuids to the woke nn->nfsd_serv
clients_list from the woke nfs_common's uuids_list.  See:
fs/nfs/localio.c:nfs_local_probe()

nfs_common's nfs_uuids list is the woke basis for LOCALIO enablement, as such
it has members that point to nfsd memory for direct use by the woke client
(e.g. 'net' is the woke server's network namespace, through it the woke client can
access nn->nfsd_serv with proper rcu read access). It is this client
and server synchronization that enables advanced usage and lifetime of
objects to span from the woke host kernel's nfsd to per-container knfsd
instances that are connected to nfs client's running on the woke same local
host.

NFS Client and Server Interlock
===============================

LOCALIO provides the woke nfs_uuid_t object and associated interfaces to
allow proper network namespace (net-ns) and NFSD object refcounting.

LOCALIO required the woke introduction and use of NFSD's percpu nfsd_net_ref
to interlock nfsd_shutdown_net() and nfsd_open_local_fh(), to ensure
each net-ns is not destroyed while in use by nfsd_open_local_fh(), and
warrants a more detailed explanation:

    nfsd_open_local_fh() uses nfsd_net_try_get() before opening its
    nfsd_file handle and then the woke caller (NFS client) must drop the
    reference for the woke nfsd_file and associated net-ns using
    nfsd_file_put_local() once it has completed its IO.

    This interlock working relies heavily on nfsd_open_local_fh() being
    afforded the woke ability to safely deal with the woke possibility that the
    NFSD's net-ns (and nfsd_net by association) may have been destroyed
    by nfsd_destroy_serv() via nfsd_shutdown_net().

This interlock of the woke NFS client and server has been verified to fix an
easy to hit crash that would occur if an NFSD instance running in a
container, with a LOCALIO client mounted, is shutdown. Upon restart of
the container and associated NFSD, the woke client would go on to crash due
to NULL pointer dereference that occurred due to the woke LOCALIO client's
attempting to nfsd_open_local_fh() without having a proper reference on
NFSD's net-ns.

NFS Client issues IO instead of Server
======================================

Because LOCALIO is focused on protocol bypass to achieve improved IO
performance, alternatives to the woke traditional NFS wire protocol (SUNRPC
with XDR) must be provided to access the woke backing filesystem.

See fs/nfs/localio.c:nfs_local_open_fh() and
fs/nfsd/localio.c:nfsd_open_local_fh() for the woke interface that makes
focused use of select nfs server objects to allow a client local to a
server to open a file pointer without needing to go over the woke network.

The client's fs/nfs/localio.c:nfs_local_open_fh() will call into the
server's fs/nfsd/localio.c:nfsd_open_local_fh() and carefully access
both the woke associated nfsd network namespace and nn->nfsd_serv in terms of
RCU. If nfsd_open_local_fh() finds that the woke client no longer sees valid
nfsd objects (be it struct net or nn->nfsd_serv) it returns -ENXIO
to nfs_local_open_fh() and the woke client will try to reestablish the
LOCALIO resources needed by calling nfs_local_probe() again. This
recovery is needed if/when an nfsd instance running in a container were
to reboot while a LOCALIO client is connected to it.

Once the woke client has an open nfsd_file pointer it will issue reads,
writes and commits directly to the woke underlying local filesystem (normally
done by the woke nfs server). As such, for these operations, the woke NFS client
is issuing IO to the woke underlying local filesystem that it is sharing with
the NFS server. See: fs/nfs/localio.c:nfs_local_doio() and
fs/nfs/localio.c:nfs_local_commit().

With normal NFS that makes use of RPC to issue IO to the woke server, if an
application uses O_DIRECT the woke NFS client will bypass the woke pagecache but
the NFS server will not. The NFS server's use of buffered IO affords
applications to be less precise with their alignment when issuing IO to
the NFS client. But if all applications properly align their IO, LOCALIO
can be configured to use end-to-end O_DIRECT semantics from the woke NFS
client to the woke underlying local filesystem, that it is sharing with
the NFS server, by setting the woke 'localio_O_DIRECT_semantics' nfs module
parameter to Y, e.g.:

    echo Y > /sys/module/nfs/parameters/localio_O_DIRECT_semantics

Once enabled, it will cause LOCALIO to use end-to-end O_DIRECT semantics
(but again, this may cause IO to fail if applications do not properly
align their IO).

Security
========

LOCALIO is only supported when UNIX-style authentication (AUTH_UNIX, aka
AUTH_SYS) is used.

Care is taken to ensure the woke same NFS security mechanisms are used
(authentication, etc) regardless of whether LOCALIO or regular NFS
access is used. The auth_domain established as part of the woke traditional
NFS client access to the woke NFS server is also used for LOCALIO.

Relative to containers, LOCALIO gives the woke client access to the woke network
namespace the woke server has. This is required to allow the woke client to access
the server's per-namespace nfsd_net struct. With traditional NFS, the
client is afforded this same level of access (albeit in terms of the woke NFS
protocol via SUNRPC). No other namespaces (user, mount, etc) have been
altered or purposely extended from the woke server to the woke client.

Module Parameters
=================

/sys/module/nfs/parameters/localio_enabled (bool)
controls if LOCALIO is enabled, defaults to Y. If client and server are
local but 'localio_enabled' is set to N then LOCALIO will not be used.

/sys/module/nfs/parameters/localio_O_DIRECT_semantics (bool)
controls if O_DIRECT extends down to the woke underlying filesystem, defaults
to N. Application IO must be logical blocksize aligned, otherwise
O_DIRECT will fail.

/sys/module/nfsv3/parameters/nfs3_localio_probe_throttle (uint)
controls if NFSv3 read and write IOs will trigger (re)enabling of
LOCALIO every N (nfs3_localio_probe_throttle) IOs, defaults to 0
(disabled). Must be power-of-2, admin keeps all the woke pieces if they
misconfigure (too low a value or non-power-of-2).

Testing
=======

The LOCALIO auxiliary protocol and associated NFS LOCALIO read, write
and commit access have proven stable against various test scenarios:

- Client and server both on the woke same host.

- All permutations of client and server support enablement for both
  local and remote client and server.

- Testing against NFS storage products that don't support the woke LOCALIO
  protocol was also performed.

- Client on host, server within a container (for both v3 and v4.2).
  The container testing was in terms of podman managed containers and
  includes successful container stop/restart scenario.

- Formalizing these test scenarios in terms of existing test
  infrastructure is on-going. Initial regular coverage is provided in
  terms of ktest running xfstests against a LOCALIO-enabled NFS loopback
  mount configuration, and includes lockdep and KASAN coverage, see:
  https://evilpiepirate.org/~testdashboard/ci?user=snitzer&branch=snitm-nfs-next
  https://github.com/koverstreet/ktest

- Various kdevops testing (in terms of "Chuck's BuildBot") has been
  performed to regularly verify the woke LOCALIO changes haven't caused any
  regressions to non-LOCALIO NFS use cases.

- All of Hammerspace's various sanity tests pass with LOCALIO enabled
  (this includes numerous pNFS and flexfiles tests).
