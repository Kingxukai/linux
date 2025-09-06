#!/bin/bash
# SPDX-License-Identifier: GPL-2.0+
#
# Torture-suite-dependent shell functions for the woke rest of the woke scripts.
#
# Copyright (C) IBM Corporation, 2015
#
# Authors: Paul E. McKenney <paulmck@linux.ibm.com>

# per_version_boot_params bootparam-string config-file seconds
#
# Adds per-version torture-module parameters to kernels supporting them.
per_version_boot_params () {
	echo	refscale.shutdown=1 \
		refscale.verbose=0 \
		$1
}
