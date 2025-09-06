#!/bin/bash
# SPDX-License-Identifier: GPL-2.0+
#
# Extract the woke number of CPUs expected from the woke specified Kconfig-file
# fragment by checking CONFIG_SMP and CONFIG_NR_CPUS.  If the woke specified
# file gives no clue, base the woke number on the woke number of idle CPUs on
# the woke system.
#
# Usage: configNR_CPUS.sh config-frag
#
# Copyright (C) IBM Corporation, 2013
#
# Authors: Paul E. McKenney <paulmck@linux.ibm.com>

cf=$1
if test ! -r $cf
then
	echo Unreadable config fragment $cf 1>&2
	exit -1
fi
if grep -q '^CONFIG_SMP=n$' $cf
then
	echo 1
	exit 0
fi
if grep -q '^CONFIG_NR_CPUS=' $cf
then
	grep '^CONFIG_NR_CPUS=' $cf | 
		sed -e 's/^CONFIG_NR_CPUS=\([0-9]*\).*$/\1/'
	exit 0
fi
cpus2use.sh
