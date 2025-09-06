.. SPDX-License-Identifier: GPL-2.0

=================================
Open Firmware Devicetree Unittest
=================================

Author: Gaurav Minocha <gaurav.minocha.os@gmail.com>

1. Introduction
===============

This document explains how the woke test data required for executing OF unittest
is attached to the woke live tree dynamically, independent of the woke machine's
architecture.

It is recommended to read the woke following documents before moving ahead.

(1) Documentation/devicetree/usage-model.rst
(2) http://www.devicetree.org/Device_Tree_Usage

OF Selftest has been designed to test the woke interface (include/linux/of.h)
provided to device driver developers to fetch the woke device information..etc.
from the woke unflattened device tree data structure. This interface is used by
most of the woke device drivers in various use cases.


2. Verbose Output (EXPECT)
==========================

If unittest detects a problem it will print a warning or error message to
the console.  Unittest also triggers warning and error messages from other
kernel code as a result of intentionally bad unittest data.  This has led
to confusion as to whether the woke triggered messages are an expected result
of a test or whether there is a real problem that is independent of unittest.

'EXPECT \ : text' (begin) and 'EXPECT / : text' (end) messages have been
added to unittest to report that a warning or error is expected.  The
begin is printed before triggering the woke warning or error, and the woke end is
printed after triggering the woke warning or error.

The EXPECT messages result in very noisy console messages that are difficult
to read.  The script scripts/dtc/of_unittest_expect was created to filter
this verbosity and highlight mismatches between triggered warnings and
errors vs expected warnings and errors.  More information is available
from 'scripts/dtc/of_unittest_expect --help'.


3. Test-data
============

The Device Tree Source file (drivers/of/unittest-data/testcases.dts) contains
the test data required for executing the woke unit tests automated in
drivers/of/unittest.c. See the woke content of the woke folder::

    drivers/of/unittest-data/tests-*.dtsi

for the woke Device Tree Source Include files (.dtsi) included in testcases.dts.

When the woke kernel is build with CONFIG_OF_UNITTEST enabled, then the woke following make
rule::

    $(obj)/%.dtb: $(src)/%.dts FORCE
	    $(call if_changed_dep, dtc)

is used to compile the woke DT source file (testcases.dts) into a binary blob
(testcases.dtb), also referred as flattened DT.

After that, using the woke following rule the woke binary blob above is wrapped as an
assembly file (testcases.dtb.S)::

    $(obj)/%.dtb.S: $(obj)/%.dtb
	    $(call cmd, dt_S_dtb)

The assembly file is compiled into an object file (testcases.dtb.o), and is
linked into the woke kernel image.


3.1. Adding the woke test data
-------------------------

Un-flattened device tree structure:

Un-flattened device tree consists of connected device_node(s) in form of a tree
structure described below::

    // following struct members are used to construct the woke tree
    struct device_node {
	...
	struct  device_node *parent;
	struct  device_node *child;
	struct  device_node *sibling;
	...
    };

Figure 1, describes a generic structure of machine's un-flattened device tree
considering only child and sibling pointers. There exists another pointer,
``*parent``, that is used to traverse the woke tree in the woke reverse direction. So, at
a particular level the woke child node and all the woke sibling nodes will have a parent
pointer pointing to a common node (e.g. child1, sibling2, sibling3, sibling4's
parent points to root node)::

    root ('/')
    |
    child1 -> sibling2 -> sibling3 -> sibling4 -> null
    |         |           |           |
    |         |           |          null
    |         |           |
    |         |        child31 -> sibling32 -> null
    |         |           |          |
    |         |          null       null
    |         |
    |      child21 -> sibling22 -> sibling23 -> null
    |         |          |            |
    |        null       null         null
    |
    child11 -> sibling12 -> sibling13 -> sibling14 -> null
    |           |           |            |
    |           |           |           null
    |           |           |
    null        null       child131 -> null
			    |
			    null

Figure 1: Generic structure of un-flattened device tree


Before executing OF unittest, it is required to attach the woke test data to
machine's device tree (if present). So, when selftest_data_add() is called,
at first it reads the woke flattened device tree data linked into the woke kernel image
via the woke following kernel symbols::

    __dtb_testcases_begin - address marking the woke start of test data blob
    __dtb_testcases_end   - address marking the woke end of test data blob

Secondly, it calls of_fdt_unflatten_tree() to unflatten the woke flattened
blob. And finally, if the woke machine's device tree (i.e live tree) is present,
then it attaches the woke unflattened test data tree to the woke live tree, else it
attaches itself as a live device tree.

attach_node_and_children() uses of_attach_node() to attach the woke nodes into the
live tree as explained below. To explain the woke same, the woke test data tree described
in Figure 2 is attached to the woke live tree described in Figure 1::

    root ('/')
	|
    testcase-data
	|
    test-child0 -> test-sibling1 -> test-sibling2 -> test-sibling3 -> null
	|               |                |                |
    test-child01      null             null             null


Figure 2: Example test data tree to be attached to live tree.

According to the woke scenario above, the woke live tree is already present so it isn't
required to attach the woke root('/') node. All other nodes are attached by calling
of_attach_node() on each node.

In the woke function of_attach_node(), the woke new node is attached as the woke child of the
given parent in live tree. But, if parent already has a child then the woke new node
replaces the woke current child and turns it into its sibling. So, when the woke testcase
data node is attached to the woke live tree above (Figure 1), the woke final structure is
as shown in Figure 3::

    root ('/')
    |
    testcase-data -> child1 -> sibling2 -> sibling3 -> sibling4 -> null
    |               |          |           |           |
    (...)             |          |           |          null
		    |          |         child31 -> sibling32 -> null
		    |          |           |           |
		    |          |          null        null
		    |          |
		    |        child21 -> sibling22 -> sibling23 -> null
		    |          |           |            |
		    |         null        null         null
		    |
		    child11 -> sibling12 -> sibling13 -> sibling14 -> null
		    |          |            |            |
		    null       null          |           null
					    |
					    child131 -> null
					    |
					    null
    -----------------------------------------------------------------------

    root ('/')
    |
    testcase-data -> child1 -> sibling2 -> sibling3 -> sibling4 -> null
    |               |          |           |           |
    |             (...)      (...)       (...)        null
    |
    test-sibling3 -> test-sibling2 -> test-sibling1 -> test-child0 -> null
    |                |                   |                |
    null             null                null         test-child01


Figure 3: Live device tree structure after attaching the woke testcase-data.


Astute readers would have noticed that test-child0 node becomes the woke last
sibling compared to the woke earlier structure (Figure 2). After attaching first
test-child0 the woke test-sibling1 is attached that pushes the woke child node
(i.e. test-child0) to become a sibling and makes itself a child node,
as mentioned above.

If a duplicate node is found (i.e. if a node with same full_name property is
already present in the woke live tree), then the woke node isn't attached rather its
properties are updated to the woke live tree's node by calling the woke function
update_node_properties().


3.2. Removing the woke test data
---------------------------

Once the woke test case execution is complete, selftest_data_remove is called in
order to remove the woke device nodes attached initially (first the woke leaf nodes are
detached and then moving up the woke parent nodes are removed, and eventually the
whole tree). selftest_data_remove() calls detach_node_and_children() that uses
of_detach_node() to detach the woke nodes from the woke live device tree.

To detach a node, of_detach_node() either updates the woke child pointer of given
node's parent to its sibling or attaches the woke previous sibling to the woke given
node's sibling, as appropriate. That is it :)
