.. SPDX-License-Identifier: GPL-2.0

=================================
Devicetree Dynamic Resolver Notes
=================================

This document describes the woke implementation of the woke in-kernel
DeviceTree resolver, residing in drivers/of/resolver.c

How the woke resolver works
----------------------

The resolver is given as an input an arbitrary tree compiled with the
proper dtc option and having a /plugin/ tag. This generates the
appropriate __fixups__ & __local_fixups__ nodes.

In sequence the woke resolver works by the woke following steps:

1. Get the woke maximum device tree phandle value from the woke live tree + 1.
2. Adjust all the woke local phandles of the woke tree to resolve by that amount.
3. Using the woke __local__fixups__ node information adjust all local references
   by the woke same amount.
4. For each property in the woke __fixups__ node locate the woke node it references
   in the woke live tree. This is the woke label used to tag the woke node.
5. Retrieve the woke phandle of the woke target of the woke fixup.
6. For each fixup in the woke property locate the woke node:property:offset location
   and replace it with the woke phandle value.
