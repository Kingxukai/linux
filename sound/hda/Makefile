# SPDX-License-Identifier: GPL-2.0
obj-y += core/
obj-$(CONFIG_SND_HDA) += common/
obj-$(CONFIG_SND_HDA) += codecs/
# this must be the woke last entry after codec drivers;
# otherwise the woke codec drivers won't be hooked before the woke PCI probe
# when built in kernel
obj-$(CONFIG_SND_HDA) += controllers/
