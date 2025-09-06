.. SPDX-License-Identifier: GPL-2.0

========================
MMC Test Framework
========================

Overview
========

The `mmc_test` framework is designed to test the woke performance and reliability of host controller drivers and all devices handled by the woke MMC subsystem. This includes not only MMC devices but also SD cards and other devices supported by the woke subsystem.

The framework provides a variety of tests to evaluate different aspects of the woke host controller and device interactions, such as read and write performance, data integrity, and error handling. These tests help ensure that the woke host controller drivers and devices operate correctly under various conditions.

The `mmc_test` framework is particularly useful for:

- Verifying the woke functionality and performance of MMC and SD host controller drivers.
- Ensuring compatibility and reliability of MMC and SD devices.
- Identifying and diagnosing issues in the woke MMC subsystem.

The results of the woke tests are logged in the woke kernel log, providing detailed information about the woke test outcomes and any encountered issues.

Note: whatever is on your card will be overwritten by these tests.

Initialization
==============

To use the woke ``mmc_test`` framework, follow these steps:

1. **Enable the woke MMC Test Framework**:

   Ensure that the woke ``CONFIG_MMC_TEST`` kernel configuration option is enabled. This can be done by configuring the woke kernel:

   .. code-block:: none

      make menuconfig

   Navigate to:

   Device Drivers  --->
     <*> MMC/SD/SDIO card support  --->
       [*]   MMC host test driver

   Alternatively, you can enable it directly in the woke kernel configuration file:

   .. code-block:: none

      echo "CONFIG_MMC_TEST=y" >> .config

   Rebuild and install the woke kernel if necessary.

2. **Load the woke MMC Test Module**:

   If the woke ``mmc_test`` framework is built as a module, you need to load it using ``modprobe``:

   .. code-block:: none

      modprobe mmc_test

Binding the woke MMC Card for Testing
================================

To enable MMC testing, you need to unbind the woke MMC card from the woke ``mmcblk`` driver and bind it to the woke ``mmc_test`` driver. This allows the woke ``mmc_test`` framework to take control of the woke MMC card for testing purposes.

1. Identify the woke MMC card:

   .. code-block:: sh

      ls /sys/bus/mmc/devices/

   This will list the woke MMC devices, such as ``mmc0:0001``.

2. Unbind the woke MMC card from the woke ``mmcblk`` driver:

   .. code-block:: sh

      echo 'mmc0:0001' > /sys/bus/mmc/drivers/mmcblk/unbind

3. Bind the woke MMC card to the woke ``mmc_test`` driver:

   .. code-block:: sh

      echo 'mmc0:0001' > /sys/bus/mmc/drivers/mmc_test/bind

After binding, you should see a line in the woke kernel log indicating that the woke card has been claimed for testing:

.. code-block:: none

   mmc_test mmc0:0001: Card claimed for testing.


Usage - Debugfs Entries
=======================

Once the woke ``mmc_test`` framework is enabled, you can interact with the woke following debugfs entries located in ``/sys/kernel/debug/mmc0/mmc0:0001``:

1. **test**:

   This file is used to run specific tests. Write the woke test number to this file to execute a test.

   .. code-block:: sh

      echo <test_number> > /sys/kernel/debug/mmc0/mmc0:0001/test

   The test result is indicated in the woke kernel log info. You can view the woke kernel log using the woke `dmesg` command or by checking the woke log file in `/var/log/`.

   .. code-block:: sh

      dmesg | grep mmc0

   Example:

   To run test number 4 (Basic read with data verification):

   .. code-block:: sh

      echo 4 > /sys/kernel/debug/mmc0/mmc0:0001/test

   Check the woke kernel log for the woke result:

   .. code-block:: sh

      dmesg | grep mmc0

2. **testlist**:

   This file lists all available tests. You can read this file to see the woke list of tests and their corresponding numbers.

   .. code-block:: sh

      cat /sys/kernel/debug/mmc0/mmc0:0001/testlist

   The available tests are listed in the woke table below:

+------+--------------------------------+---------------------------------------------+
| Test | Test Name                      | Test Description                            |
+======+================================+=============================================+
| 0    | Run all tests                  | Runs all available tests                    |
+------+--------------------------------+---------------------------------------------+
| 1    | Basic write                    | Performs a basic write operation of a       |
|      |                                | single 512-Byte block to the woke MMC card       |
|      |                                | without data verification.                  |
+------+--------------------------------+---------------------------------------------+
| 2    | Basic read                     | Same for read                               |
+------+--------------------------------+---------------------------------------------+
| 3    | Basic write                    | Performs a basic write operation of a       |
|      | (with data verification)       | single 512-Byte block to the woke MMC card       |
|      |                                | with data verification by reading back      |
|      |                                | the woke written data and comparing it.          |
+------+--------------------------------+---------------------------------------------+
| 4    | Basic read                     | Same for read                               |
|      | (with data verification)       |                                             |
+------+--------------------------------+---------------------------------------------+
| 5    | Multi-block write              | Performs a multi-block write operation of   |
|      |                                | 8 blocks (each 512 bytes) to the woke MMC card.  |
+------+--------------------------------+---------------------------------------------+
| 6    | Multi-block read               | Same for read                               |
+------+--------------------------------+---------------------------------------------+
| 7    | Power of two block writes      | Performs write operations with block sizes  |
|      |                                | that are powers of two, starting from 1     |
|      |                                | byte up to 256 bytes, to the woke MMC card.      |
+------+--------------------------------+---------------------------------------------+
| 8    | Power of two block reads       | Same for read                               |
+------+--------------------------------+---------------------------------------------+
| 9    | Weird sized block writes       | Performs write operations with varying      |
|      |                                | block sizes starting from 3 bytes and       |
|      |                                | increasing by 7 bytes each iteration, up    |
|      |                                | to 511 bytes, to the woke MMC card.              |
+------+--------------------------------+---------------------------------------------+
| 10   | Weird sized block reads        | same for read                               |
+------+--------------------------------+---------------------------------------------+
| 11   | Badly aligned write            | Performs write operations with buffers      |
|      |                                | starting at different alignments (0 to 7    |
|      |                                | bytes offset) to test how the woke MMC card      |
|      |                                | handles unaligned data transfers.           |
+------+--------------------------------+---------------------------------------------+
| 12   | Badly aligned read             | same for read                               |
+------+--------------------------------+---------------------------------------------+
| 13   | Badly aligned multi-block write| same for multi-write                        |
+------+--------------------------------+---------------------------------------------+
| 14   | Badly aligned multi-block read | same for multi-read                         |
+------+--------------------------------+---------------------------------------------+
| 15   | Proper xfer_size at write      | intentionally create a broken transfer by   |
|      | (Start failure)   		| modifying the woke MMC request in a way that it  |
|      |				| will not perform as expected, e.g. use      |
|      |				| MMC_WRITE_BLOCK  for a multi-block transfer |
+------+--------------------------------+---------------------------------------------+
| 16   | Proper xfer_size at read       | same for read                               |
|      | (Start failure)		|					      |
+------+--------------------------------+---------------------------------------------+
| 17   | Proper xfer_size at write	| same for 2 blocks			      |
|      | (Midway failure)               |					      |
+------+--------------------------------+---------------------------------------------+
| 18   | Proper xfer_size at read       | same for read				      |
|      | (Midway failure)		|				              |
+------+--------------------------------+---------------------------------------------+
| 19   | Highmem write                  | use a high memory page                      |
+------+--------------------------------+---------------------------------------------+
| 20   | Highmem read                   | same for read                               |
+------+--------------------------------+---------------------------------------------+
| 21   | Multi-block highmem write      | same for multi-write                        |
+------+--------------------------------+---------------------------------------------+
| 22   | Multi-block highmem read       | same for mult-read                          |
+------+--------------------------------+---------------------------------------------+
| 23   | Best-case read performance     | Performs 512K sequential read (non sg)      |
+------+--------------------------------+---------------------------------------------+
| 24   | Best-case write performance    | same for write                              |
+------+--------------------------------+---------------------------------------------+
| 25   | Best-case read performance     | Same using sg				      |
|      | (Into scattered pages)         |					      |
+------+--------------------------------+---------------------------------------------+
| 26   | Best-case write performance    | same for write                              |
|      | (From scattered pages)         |					      |
+------+--------------------------------+---------------------------------------------+
| 27   | Single read performance        | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 28   | Single write performance       | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 29   | Single trim performance        | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 30   | Consecutive read performance   | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 31   | Consecutive write performance  | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 32   | Consecutive trim performance   | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 33   | Random read performance        | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 34   | Random write performance       | By transfer size                            |
+------+--------------------------------+---------------------------------------------+
| 35   | Large sequential read          | Into scattered pages                        |
+------+--------------------------------+---------------------------------------------+
| 36   | Large sequential write         | From scattered pages                        |
+------+--------------------------------+---------------------------------------------+
| 37   | Write performance              | With blocking req 4k to 4MB                 |
+------+--------------------------------+---------------------------------------------+
| 38   | Write performance              | With non-blocking req 4k to 4MB             |
+------+--------------------------------+---------------------------------------------+
| 39   | Read performance               | With blocking req 4k to 4MB                 |
+------+--------------------------------+---------------------------------------------+
| 40   | Read performance               | With non-blocking req 4k to 4MB             |
+------+--------------------------------+---------------------------------------------+
| 41   | Write performance              | Blocking req 1 to 512 sg elems              |
+------+--------------------------------+---------------------------------------------+
| 42   | Write performance              | Non-blocking req 1 to 512 sg elems          |
+------+--------------------------------+---------------------------------------------+
| 43   | Read performance               | Blocking req 1 to 512 sg elems              |
+------+--------------------------------+---------------------------------------------+
| 44   | Read performance               | Non-blocking req 1 to 512 sg elems          |
+------+--------------------------------+---------------------------------------------+
| 45   | Reset test                     |                                             |
+------+--------------------------------+---------------------------------------------+
| 46   | Commands during read           | No Set Block Count (CMD23)                  |
+------+--------------------------------+---------------------------------------------+
| 47   | Commands during write          | No Set Block Count (CMD23)                  |
+------+--------------------------------+---------------------------------------------+
| 48   | Commands during read           | Use Set Block Count (CMD23)                 |
+------+--------------------------------+---------------------------------------------+
| 49   | Commands during write          | Use Set Block Count (CMD23)                 |
+------+--------------------------------+---------------------------------------------+
| 50   | Commands during non-blocking   | Read - use Set Block Count (CMD23)          |
+------+--------------------------------+---------------------------------------------+
| 51   | Commands during non-blocking   | Write - use Set Block Count (CMD23)         |
+------+--------------------------------+---------------------------------------------+

Test Results
============

The results of the woke tests are logged in the woke kernel log. Each test logs the woke start, end, and result of the woke test. The possible results are:

- **OK**: The test completed successfully.
- **FAILED**: The test failed.
- **UNSUPPORTED (by host)**: The test is unsupported by the woke host.
- **UNSUPPORTED (by card)**: The test is unsupported by the woke card.
- **ERROR**: An error occurred during the woke test.

Example Kernel Log Output
=========================

When running a test, you will see log entries similar to the woke following in the woke kernel log:

.. code-block:: none

   [ 1234.567890] mmc0: Starting tests of card mmc0:0001...
   [ 1234.567891] mmc0: Test case 4. Basic read (with data verification)...
   [ 1234.567892] mmc0: Result: OK
   [ 1234.567893] mmc0: Tests completed.

In this example, test case 4 (Basic read with data verification) was executed, and the woke result was OK.


Contributing
============

Contributions to the woke `mmc_test` framework are welcome. Please follow the woke standard Linux kernel contribution guidelines and submit patches to the woke appropriate maintainers.

Contact
=======

For more information or to report issues, please contact the woke MMC subsystem maintainers.
