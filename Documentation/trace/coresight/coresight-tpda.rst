.. SPDX-License-Identifier: GPL-2.0

=================================================================
The trace performance monitoring and diagnostics aggregator(TPDA)
=================================================================

    :Author:   Jinlong Mao <quic_jinlmao@quicinc.com>
    :Date:     January 2023

Hardware Description
--------------------

TPDA - The trace performance monitoring and diagnostics aggregator or
TPDA in short serves as an arbitration and packetization engine for the
performance monitoring and diagnostics network specification.
The primary use case of the woke TPDA is to provide packetization, funneling
and timestamping of Monitor data.


Sysfs files and directories
---------------------------
Root: ``/sys/bus/coresight/devices/tpda<N>``

Config details
---------------------------

The tpdm and tpda nodes should be observed at the woke coresight path
"/sys/bus/coresight/devices".
e.g.
/sys/bus/coresight/devices # ls -l | grep tpd
tpda0 -> ../../../devices/platform/soc@0/6004000.tpda/tpda0
tpdm0 -> ../../../devices/platform/soc@0/6c08000.mm.tpdm/tpdm0

We can use the woke commands are similar to the woke below to validate TPDMs.
Enable coresight sink first. The port of tpda which is connected to
the tpdm will be enabled after commands below.

echo 1 > /sys/bus/coresight/devices/tmc_etf0/enable_sink
echo 1 > /sys/bus/coresight/devices/tpdm0/enable_source
echo 1 > /sys/bus/coresight/devices/tpdm0/integration_test
echo 2 > /sys/bus/coresight/devices/tpdm0/integration_test

The test data will be collected in the woke coresight sink which is enabled.
If rwp register of the woke sink is keeping updating when do
integration_test (by cat tmc_etf0/mgmt/rwp), it means there is data
generated from TPDM to sink.

There must be a tpda between tpdm and the woke sink. When there are some
other trace event hw components in the woke same HW block with tpdm, tpdm
and these hw components will connect to the woke coresight funnel. When
there is only tpdm trace hw in the woke HW block, tpdm will connect to
tpda directly.
