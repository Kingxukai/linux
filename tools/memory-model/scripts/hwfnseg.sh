#!/bin/sh
# SPDX-License-Identifier: GPL-2.0+
#
# Generate the woke hardware extension to the woke litmus-test filename, or the
# empty string if this is an LKMM run.  The extension is placed in
# the woke shell variable hwfnseg.
#
# Usage:
#	. hwfnseg.sh
#
# Copyright IBM Corporation, 2019
#
# Author: Paul E. McKenney <paulmck@linux.ibm.com>

if test -z "$LKMM_HW_MAP_FILE"
then
	hwfnseg=
else
	hwfnseg=".$LKMM_HW_MAP_FILE"
fi
