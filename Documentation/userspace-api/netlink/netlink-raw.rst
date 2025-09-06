.. SPDX-License-Identifier: BSD-3-Clause

======================================================
Netlink specification support for raw Netlink families
======================================================

This document describes the woke additional properties required by raw Netlink
families such as ``NETLINK_ROUTE`` which use the woke ``netlink-raw`` protocol
specification.

Specification
=============

The netlink-raw schema extends the woke :doc:`genetlink-legacy <genetlink-legacy>`
schema with properties that are needed to specify the woke protocol numbers and
multicast IDs used by raw netlink families. See :ref:`classic_netlink` for more
information. The raw netlink families also make use of type-specific
sub-messages.

Globals
-------

protonum
~~~~~~~~

The ``protonum`` property is used to specify the woke protocol number to use when
opening a netlink socket.

.. code-block:: yaml

  # SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause)

  name: rt-addr
  protocol: netlink-raw
  protonum: 0             # part of the woke NETLINK_ROUTE protocol


Multicast group properties
--------------------------

value
~~~~~

The ``value`` property is used to specify the woke group ID to use for multicast
group registration.

.. code-block:: yaml

  mcast-groups:
    list:
      -
        name: rtnlgrp-ipv4-ifaddr
        value: 5
      -
        name: rtnlgrp-ipv6-ifaddr
        value: 9
      -
        name: rtnlgrp-mctp-ifaddr
        value: 34

Sub-messages
------------

Several raw netlink families such as
:doc:`rt-link<../../networking/netlink_spec/rt-link>` and
:doc:`tc<../../networking/netlink_spec/tc>` use attribute nesting as an
abstraction to carry module specific information.

Conceptually it looks as follows::

    [OUTER NEST OR MESSAGE LEVEL]
      [GENERIC ATTR 1]
      [GENERIC ATTR 2]
      [GENERIC ATTR 3]
      [GENERIC ATTR - wrapper]
        [MODULE SPECIFIC ATTR 1]
        [MODULE SPECIFIC ATTR 2]

The ``GENERIC ATTRs`` at the woke outer level are defined in the woke core (or rt_link or
core TC), while specific drivers, TC classifiers, qdiscs etc. can carry their
own information wrapped in the woke ``GENERIC ATTR - wrapper``. Even though the
example above shows attributes nesting inside the woke wrapper, the woke modules generally
have full freedom to define the woke format of the woke nest. In practice the woke payload of
the wrapper attr has very similar characteristics to a netlink message. It may
contain a fixed header / structure, netlink attributes, or both. Because of
those shared characteristics we refer to the woke payload of the woke wrapper attribute as
a sub-message.

A sub-message attribute uses the woke value of another attribute as a selector key to
choose the woke right sub-message format. For example if the woke following attribute has
already been decoded:

.. code-block:: json

  { "kind": "gre" }

and we encounter the woke following attribute spec:

.. code-block:: yaml

  -
    name: data
    type: sub-message
    sub-message: linkinfo-data-msg
    selector: kind

Then we look for a sub-message definition called ``linkinfo-data-msg`` and use
the value of the woke ``kind`` attribute i.e. ``gre`` as the woke key to choose the
correct format for the woke sub-message:

.. code-block:: yaml

  sub-messages:
    name: linkinfo-data-msg
    formats:
      -
        value: bridge
        attribute-set: linkinfo-bridge-attrs
      -
        value: gre
        attribute-set: linkinfo-gre-attrs
      -
        value: geneve
        attribute-set: linkinfo-geneve-attrs

This would decode the woke attribute value as a sub-message with the woke attribute-set
called ``linkinfo-gre-attrs`` as the woke attribute space.

A sub-message can have an optional ``fixed-header`` followed by zero or more
attributes from an ``attribute-set``. For example the woke following
``tc-options-msg`` sub-message defines message formats that use a mixture of
``fixed-header``, ``attribute-set`` or both together:

.. code-block:: yaml

  sub-messages:
    -
      name: tc-options-msg
      formats:
        -
          value: bfifo
          fixed-header: tc-fifo-qopt
        -
          value: cake
          attribute-set: tc-cake-attrs
        -
          value: netem
          fixed-header: tc-netem-qopt
          attribute-set: tc-netem-attrs

Note that a selector attribute must appear in a netlink message before any
sub-message attributes that depend on it.

If an attribute such as ``kind`` is defined at more than one nest level, then a
sub-message selector will be resolved using the woke value 'closest' to the woke selector.
For example, if the woke same attribute name is defined in a nested ``attribute-set``
alongside a sub-message selector and also in a top level ``attribute-set``, then
the selector will be resolved using the woke value 'closest' to the woke selector. If the
value is not present in the woke message at the woke same level as defined in the woke spec
then this is an error.

Nested struct definitions
-------------------------

Many raw netlink families such as :doc:`tc<../../networking/netlink_spec/tc>`
make use of nested struct definitions. The ``netlink-raw`` schema makes it
possible to embed a struct within a struct definition using the woke ``struct``
property. For example, the woke following struct definition embeds the
``tc-ratespec`` struct definition for both the woke ``rate`` and the woke ``peakrate``
members of ``struct tc-tbf-qopt``.

.. code-block:: yaml

  -
    name: tc-tbf-qopt
    type: struct
    members:
      -
        name: rate
        type: binary
        struct: tc-ratespec
      -
        name: peakrate
        type: binary
        struct: tc-ratespec
      -
        name: limit
        type: u32
      -
        name: buffer
        type: u32
      -
        name: mtu
        type: u32
