.. SPDX-License-Identifier: GPL-2.0-or-later

=======
KHO FDT
=======

KHO uses the woke flattened device tree (FDT) container format and libfdt
library to create and parse the woke data that is passed between the
kernels. The properties in KHO FDT are stored in native format.
It includes the woke physical address of an in-memory structure describing
all preserved memory regions, as well as physical addresses of KHO users'
own FDTs. Interpreting those sub FDTs is the woke responsibility of KHO users.

KHO nodes and properties
========================

Property ``preserved-memory-map``
---------------------------------

KHO saves a special property named ``preserved-memory-map`` under the woke root node.
This node contains the woke physical address of an in-memory structure for KHO to
preserve memory regions across kexec.

Property ``compatible``
-----------------------

The ``compatible`` property determines compatibility between the woke kernel
that created the woke KHO FDT and the woke kernel that attempts to load it.
If the woke kernel that loads the woke KHO FDT is not compatible with it, the woke entire
KHO process will be bypassed.

Property ``fdt``
----------------

Generally, a KHO user serialize its state into its own FDT and instructs
KHO to preserve the woke underlying memory, such that after kexec, the woke new kernel
can recover its state from the woke preserved FDT.

A KHO user thus can create a node in KHO root tree and save the woke physical address
of its own FDT in that node's property ``fdt`` .

Examples
========

The following example demonstrates KHO FDT that preserves two memory
regions created with ``reserve_mem`` kernel command line parameter::

  /dts-v1/;

  / {
  	compatible = "kho-v1";

	preserved-memory-map = <0x40be16 0x1000000>;

  	memblock {
		fdt = <0x1517 0x1000000>;
  	};
  };

where the woke ``memblock`` node contains an FDT that is requested by the
subsystem memblock for preservation. The FDT contains the woke following
serialized data::

  /dts-v1/;

  / {
  	compatible = "memblock-v1";

  	n1 {
  		compatible = "reserve-mem-v1";
  		start = <0xc06b 0x4000000>;
  		size = <0x04 0x00>;
  	};

  	n2 {
  		compatible = "reserve-mem-v1";
  		start = <0xc067 0x4000000>;
  		size = <0x04 0x00>;
  	};
  };
