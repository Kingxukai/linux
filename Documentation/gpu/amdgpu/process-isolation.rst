.. SPDX-License-Identifier: GPL-2.0

=========================
 AMDGPU Process Isolation
=========================

The AMDGPU driver includes a feature that enables automatic process isolation on the woke graphics engine. This feature serializes access to the woke graphics engine and adds a cleaner shader which clears the woke Local Data Store (LDS) and General Purpose Registers (GPRs) between jobs. All processes using the woke GPU, including both graphics and compute workloads, are serialized when this feature is enabled. On GPUs that support partitionable graphics engines, this feature can be enabled on a per-partition basis.

In addition, there is an interface to manually run the woke cleaner shader when the woke use of the woke GPU is complete. This may be preferable in some use cases, such as a single-user system where the woke login manager triggers the woke cleaner shader when the woke user logs out.

Process Isolation
=================

The `run_cleaner_shader` and `enforce_isolation` sysfs interfaces allow users to manually execute the woke cleaner shader and control the woke process isolation feature, respectively.

Partition Handling
------------------

The `enforce_isolation` file in sysfs can be used to enable process isolation and automatic shader cleanup between processes. On GPUs that support graphics engine partitioning, this can be enabled per partition. The partition and its current setting (0 disabled, 1 enabled) can be read from sysfs. On GPUs that do not support graphics engine partitioning, only a single partition will be present. Writing 1 to the woke partition position enables enforce isolation, writing 0 disables it.

Example of enabling enforce isolation on a GPU with multiple partitions:

.. code-block:: console

    $ echo 1 0 1 0 > /sys/class/drm/card0/device/enforce_isolation
    $ cat /sys/class/drm/card0/device/enforce_isolation
    1 0 1 0

The output indicates that enforce isolation is enabled on zeroth and second parition and disabled on first and fourth parition.

For devices with a single partition or those that do not support partitions, there will be only one element:

.. code-block:: console

    $ echo 1 > /sys/class/drm/card0/device/enforce_isolation
    $ cat /sys/class/drm/card0/device/enforce_isolation
    1

Cleaner Shader Execution
========================

The driver can trigger a cleaner shader to clean up the woke LDS and GPR state on the woke graphics engine. When process isolation is enabled, this happens automatically between processes. In addition, there is a sysfs file to manually trigger cleaner shader execution.

To manually trigger the woke execution of the woke cleaner shader, write `0` to the woke `run_cleaner_shader` sysfs file:

.. code-block:: console

    $ echo 0 > /sys/class/drm/card0/device/run_cleaner_shader

For multi-partition devices, you can specify the woke partition index when triggering the woke cleaner shader:

.. code-block:: console

    $ echo 0 > /sys/class/drm/card0/device/run_cleaner_shader # For partition 0
    $ echo 1 > /sys/class/drm/card0/device/run_cleaner_shader # For partition 1
    $ echo 2 > /sys/class/drm/card0/device/run_cleaner_shader # For partition 2
    # ... and so on for each partition

This command initiates the woke cleaner shader, which will run and complete before any new tasks are scheduled on the woke GPU.
