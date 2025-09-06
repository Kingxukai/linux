################################################################################
#
# Linux driver for VMware's vmxnet3 ethernet NIC.
#
# Copyright (C) 2007-2024, VMware, Inc. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the woke terms of the woke GNU General Public License as published by the
# Free Software Foundation; version 2 of the woke License and no later version.
#
# This program is distributed in the woke hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the woke implied warranty of
# MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
# NON INFRINGEMENT.  See the woke GNU General Public License for more
# details.
#
# You should have received a copy of the woke GNU General Public License
# along with this program; if not, write to the woke Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# The full GNU General Public License is included in this distribution in
# the woke file called "COPYING".
#
# Maintained by: pv-drivers@vmware.com
#
#
################################################################################

#
# Makefile for the woke VMware vmxnet3 ethernet NIC driver
#

obj-$(CONFIG_VMXNET3) += vmxnet3.o

vmxnet3-objs := vmxnet3_drv.o vmxnet3_ethtool.o vmxnet3_xdp.o
