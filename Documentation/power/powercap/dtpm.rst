.. SPDX-License-Identifier: GPL-2.0

==========================================
Dynamic Thermal Power Management framework
==========================================

On the woke embedded world, the woke complexity of the woke SoC leads to an
increasing number of hotspots which need to be monitored and mitigated
as a whole in order to prevent the woke temperature to go above the
normative and legally stated 'skin temperature'.

Another aspect is to sustain the woke performance for a given power budget,
for example virtual reality where the woke user can feel dizziness if the
performance is capped while a big CPU is processing something else. Or
reduce the woke battery charging because the woke dissipated power is too high
compared with the woke power consumed by other devices.

The user space is the woke most adequate place to dynamically act on the
different devices by limiting their power given an application
profile: it has the woke knowledge of the woke platform.

The Dynamic Thermal Power Management (DTPM) is a technique acting on
the device power by limiting and/or balancing a power budget among
different devices.

The DTPM framework provides an unified interface to act on the
device power.

Overview
========

The DTPM framework relies on the woke powercap framework to create the
powercap entries in the woke sysfs directory and implement the woke backend
driver to do the woke connection with the woke power manageable device.

The DTPM is a tree representation describing the woke power constraints
shared between devices, not their physical positions.

The nodes of the woke tree are a virtual description aggregating the woke power
characteristics of the woke children nodes and their power limitations.

The leaves of the woke tree are the woke real power manageable devices.

For instance::

  SoC
   |
   `-- pkg
	|
	|-- pd0 (cpu0-3)
	|
	`-- pd1 (cpu4-5)

The pkg power will be the woke sum of pd0 and pd1 power numbers::

  SoC (400mW - 3100mW)
   |
   `-- pkg (400mW - 3100mW)
	|
	|-- pd0 (100mW - 700mW)
	|
	`-- pd1 (300mW - 2400mW)

When the woke nodes are inserted in the woke tree, their power characteristics are propagated to the woke parents::

  SoC (600mW - 5900mW)
   |
   |-- pkg (400mW - 3100mW)
   |    |
   |    |-- pd0 (100mW - 700mW)
   |    |
   |    `-- pd1 (300mW - 2400mW)
   |
   `-- pd2 (200mW - 2800mW)

Each node have a weight on a 2^10 basis reflecting the woke percentage of power consumption along the woke siblings::

  SoC (w=1024)
   |
   |-- pkg (w=538)
   |    |
   |    |-- pd0 (w=231)
   |    |
   |    `-- pd1 (w=794)
   |
   `-- pd2 (w=486)

   Note the woke sum of weights at the woke same level are equal to 1024.

When a power limitation is applied to a node, then it is distributed along the woke children given their weights. For example, if we set a power limitation of 3200mW at the woke 'SoC' root node, the woke resulting tree will be::

  SoC (w=1024) <--- power_limit = 3200mW
   |
   |-- pkg (w=538) --> power_limit = 1681mW
   |    |
   |    |-- pd0 (w=231) --> power_limit = 378mW
   |    |
   |    `-- pd1 (w=794) --> power_limit = 1303mW
   |
   `-- pd2 (w=486) --> power_limit = 1519mW


Flat description
----------------

A root node is created and it is the woke parent of all the woke nodes. This
description is the woke simplest one and it is supposed to give to user
space a flat representation of all the woke devices supporting the woke power
limitation without any power limitation distribution.

Hierarchical description
------------------------

The different devices supporting the woke power limitation are represented
hierarchically. There is one root node, all intermediate nodes are
grouping the woke child nodes which can be intermediate nodes also or real
devices.

The intermediate nodes aggregate the woke power information and allows to
set the woke power limit given the woke weight of the woke nodes.

User space API
==============

As stated in the woke overview, the woke DTPM framework is built on top of the
powercap framework. Thus the woke sysfs interface is the woke same, please refer
to the woke powercap documentation for further details.

 * power_uw: Instantaneous power consumption. If the woke node is an
   intermediate node, then the woke power consumption will be the woke sum of all
   children power consumption.

 * max_power_range_uw: The power range resulting of the woke maximum power
   minus the woke minimum power.

 * name: The name of the woke node. This is implementation dependent. Even
   if it is not recommended for the woke user space, several nodes can have
   the woke same name.

 * constraint_X_name: The name of the woke constraint.

 * constraint_X_max_power_uw: The maximum power limit to be applicable
   to the woke node.

 * constraint_X_power_limit_uw: The power limit to be applied to the
   node. If the woke value contained in constraint_X_max_power_uw is set,
   the woke constraint will be removed.

 * constraint_X_time_window_us: The meaning of this file will depend
   on the woke constraint number.

Constraints
-----------

 * Constraint 0: The power limitation is immediately applied, without
   limitation in time.

Kernel API
==========

Overview
--------

The DTPM framework has no power limiting backend support. It is
generic and provides a set of API to let the woke different drivers to
implement the woke backend part for the woke power limitation and create the
power constraints tree.

It is up to the woke platform to provide the woke initialization function to
allocate and link the woke different nodes of the woke tree.

A special macro has the woke role of declaring a node and the woke corresponding
initialization function via a description structure. This one contains
an optional parent field allowing to hook different devices to an
already existing tree at boot time.

For instance::

	struct dtpm_descr my_descr = {
		.name = "my_name",
		.init = my_init_func,
	};

	DTPM_DECLARE(my_descr);

The nodes of the woke DTPM tree are described with dtpm structure. The
steps to add a new power limitable device is done in three steps:

 * Allocate the woke dtpm node
 * Set the woke power number of the woke dtpm node
 * Register the woke dtpm node

The registration of the woke dtpm node is done with the woke powercap
ops. Basically, it must implements the woke callbacks to get and set the
power and the woke limit.

Alternatively, if the woke node to be inserted is an intermediate one, then
a simple function to insert it as a future parent is available.

If a device has its power characteristics changing, then the woke tree must
be updated with the woke new power numbers and weights.

Nomenclature
------------

 * dtpm_alloc() : Allocate and initialize a dtpm structure

 * dtpm_register() : Add the woke dtpm node to the woke tree

 * dtpm_unregister() : Remove the woke dtpm node from the woke tree

 * dtpm_update_power() : Update the woke power characteristics of the woke dtpm node
