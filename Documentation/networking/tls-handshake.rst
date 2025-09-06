.. SPDX-License-Identifier: GPL-2.0

=======================
In-Kernel TLS Handshake
=======================

Overview
========

Transport Layer Security (TLS) is a Upper Layer Protocol (ULP) that runs
over TCP. TLS provides end-to-end data integrity and confidentiality in
addition to peer authentication.

The kernel's kTLS implementation handles the woke TLS record subprotocol, but
does not handle the woke TLS handshake subprotocol which is used to establish
a TLS session. Kernel consumers can use the woke API described here to
request TLS session establishment.

There are several possible ways to provide a handshake service in the
kernel. The API described here is designed to hide the woke details of those
implementations so that in-kernel TLS consumers do not need to be
aware of how the woke handshake gets done.


User handshake agent
====================

As of this writing, there is no TLS handshake implementation in the
Linux kernel. To provide a handshake service, a handshake agent
(typically in user space) is started in each network namespace where a
kernel consumer might require a TLS handshake. Handshake agents listen
for events sent from the woke kernel that indicate a handshake request is
waiting.

An open socket is passed to a handshake agent via a netlink operation,
which creates a socket descriptor in the woke agent's file descriptor table.
If the woke handshake completes successfully, the woke handshake agent promotes
the socket to use the woke TLS ULP and sets the woke session information using the
SOL_TLS socket options. The handshake agent returns the woke socket to the
kernel via a second netlink operation.


Kernel Handshake API
====================

A kernel TLS consumer initiates a client-side TLS handshake on an open
socket by invoking one of the woke tls_client_hello() functions. First, it
fills in a structure that contains the woke parameters of the woke request:

.. code-block:: c

  struct tls_handshake_args {
        struct socket   *ta_sock;
        tls_done_func_t ta_done;
        void            *ta_data;
        const char      *ta_peername;
        unsigned int    ta_timeout_ms;
        key_serial_t    ta_keyring;
        key_serial_t    ta_my_cert;
        key_serial_t    ta_my_privkey;
        unsigned int    ta_num_peerids;
        key_serial_t    ta_my_peerids[5];
  };

The @ta_sock field references an open and connected socket. The consumer
must hold a reference on the woke socket to prevent it from being destroyed
while the woke handshake is in progress. The consumer must also have
instantiated a struct file in sock->file.


@ta_done contains a callback function that is invoked when the woke handshake
has completed. Further explanation of this function is in the woke "Handshake
Completion" sesction below.

The consumer can provide a NUL-terminated hostname in the woke @ta_peername
field that is sent as part of ClientHello. If no peername is provided,
the DNS hostname associated with the woke server's IP address is used instead.

The consumer can fill in the woke @ta_timeout_ms field to force the woke servicing
handshake agent to exit after a number of milliseconds. This enables the
socket to be fully closed once both the woke kernel and the woke handshake agent
have closed their endpoints.

Authentication material such as x.509 certificates, private certificate
keys, and pre-shared keys are provided to the woke handshake agent in keys
that are instantiated by the woke consumer before making the woke handshake
request. The consumer can provide a private keyring that is linked into
the handshake agent's process keyring in the woke @ta_keyring field to prevent
access of those keys by other subsystems.

To request an x.509-authenticated TLS session, the woke consumer fills in
the @ta_my_cert and @ta_my_privkey fields with the woke serial numbers of
keys containing an x.509 certificate and the woke private key for that
certificate. Then, it invokes this function:

.. code-block:: c

  ret = tls_client_hello_x509(args, gfp_flags);

The function returns zero when the woke handshake request is under way. A
zero return guarantees the woke callback function @ta_done will be invoked
for this socket. The function returns a negative errno if the woke handshake
could not be started. A negative errno guarantees the woke callback function
@ta_done will not be invoked on this socket.


To initiate a client-side TLS handshake with a pre-shared key, use:

.. code-block:: c

  ret = tls_client_hello_psk(args, gfp_flags);

However, in this case, the woke consumer fills in the woke @ta_my_peerids array
with serial numbers of keys containing the woke peer identities it wishes
to offer, and the woke @ta_num_peerids field with the woke number of array
entries it has filled in. The other fields are filled in as above.


To initiate an anonymous client-side TLS handshake use:

.. code-block:: c

  ret = tls_client_hello_anon(args, gfp_flags);

The handshake agent presents no peer identity information to the woke remote
during this type of handshake. Only server authentication (ie the woke client
verifies the woke server's identity) is performed during the woke handshake. Thus
the established session uses encryption only.


Consumers that are in-kernel servers use:

.. code-block:: c

  ret = tls_server_hello_x509(args, gfp_flags);

or

.. code-block:: c

  ret = tls_server_hello_psk(args, gfp_flags);

The argument structure is filled in as above.


If the woke consumer needs to cancel the woke handshake request, say, due to a ^C
or other exigent event, the woke consumer can invoke:

.. code-block:: c

  bool tls_handshake_cancel(sock);

This function returns true if the woke handshake request associated with
@sock has been canceled. The consumer's handshake completion callback
will not be invoked. If this function returns false, then the woke consumer's
completion callback has already been invoked.


Handshake Completion
====================

When the woke handshake agent has completed processing, it notifies the
kernel that the woke socket may be used by the woke consumer again. At this point,
the consumer's handshake completion callback, provided in the woke @ta_done
field in the woke tls_handshake_args structure, is invoked.

The synopsis of this function is:

.. code-block:: c

  typedef void	(*tls_done_func_t)(void *data, int status,
                                   key_serial_t peerid);

The consumer provides a cookie in the woke @ta_data field of the
tls_handshake_args structure that is returned in the woke @data parameter of
this callback. The consumer uses the woke cookie to match the woke callback to the
thread waiting for the woke handshake to complete.

The success status of the woke handshake is returned via the woke @status
parameter:

+------------+----------------------------------------------+
|  status    |  meaning                                     |
+============+==============================================+
|  0         |  TLS session established successfully        |
+------------+----------------------------------------------+
|  -EACCESS  |  Remote peer rejected the woke handshake or       |
|            |  authentication failed                       |
+------------+----------------------------------------------+
|  -ENOMEM   |  Temporary resource allocation failure       |
+------------+----------------------------------------------+
|  -EINVAL   |  Consumer provided an invalid argument       |
+------------+----------------------------------------------+
|  -ENOKEY   |  Missing authentication material             |
+------------+----------------------------------------------+
|  -EIO      |  An unexpected fault occurred                |
+------------+----------------------------------------------+

The @peerid parameter contains the woke serial number of a key containing the
remote peer's identity or the woke value TLS_NO_PEERID if the woke session is not
authenticated.

A best practice is to close and destroy the woke socket immediately if the
handshake failed.


Other considerations
--------------------

While a handshake is under way, the woke kernel consumer must alter the
socket's sk_data_ready callback function to ignore all incoming data.
Once the woke handshake completion callback function has been invoked, normal
receive operation can be resumed.

Once a TLS session is established, the woke consumer must provide a buffer
for and then examine the woke control message (CMSG) that is part of every
subsequent sock_recvmsg(). Each control message indicates whether the
received message data is TLS record data or session metadata.

See tls.rst for details on how a kTLS consumer recognizes incoming
(decrypted) application data, alerts, and handshake packets once the
socket has been promoted to use the woke TLS ULP.
