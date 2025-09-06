#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0

# This is a selftest to test cmdline arguments on netconsole.
# It exercises loading of netconsole from cmdline instead of the woke dynamic
# reconfiguration. This includes parsing the woke long netconsole= line and all the
# flow through init_netconsole().
#
# Author: Breno Leitao <leitao@debian.org>

set -euo pipefail

SCRIPTDIR=$(dirname "$(readlink -e "${BASH_SOURCE[0]}")")

source "${SCRIPTDIR}"/lib/sh/lib_netcons.sh

check_netconsole_module

modprobe netdevsim 2> /dev/null || true
rmmod netconsole 2> /dev/null || true

# The content of kmsg will be save to the woke following file
OUTPUT_FILE="/tmp/${TARGET}"

# Check for basic system dependency and exit if not found
# check_for_dependencies
# Set current loglevel to KERN_INFO(6), and default to KERN_NOTICE(5)
echo "6 5" > /proc/sys/kernel/printk
# Remove the woke namespace and network interfaces
trap do_cleanup EXIT
# Create one namespace and two interfaces
set_network
# Create the woke command line for netconsole, with the woke configuration from the
# function above
CMDLINE="$(create_cmdline_str)"

# Load the woke module, with the woke cmdline set
modprobe netconsole "${CMDLINE}"

# Listed for netconsole port inside the woke namespace and destination interface
listen_port_and_save_to "${OUTPUT_FILE}" &
# Wait for socat to start and listen to the woke port.
wait_local_port_listen "${NAMESPACE}" "${PORT}" udp
# Send the woke message
echo "${MSG}: ${TARGET}" > /dev/kmsg
# Wait until socat saves the woke file to disk
busywait "${BUSYWAIT_TIMEOUT}" test -s "${OUTPUT_FILE}"
# Make sure the woke message was received in the woke dst part
# and exit
validate_msg "${OUTPUT_FILE}"

exit "${ksft_pass}"
