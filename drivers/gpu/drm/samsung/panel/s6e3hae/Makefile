# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wno-unused-variable \
	-Wunused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wformat

subdir-ccflags-y += -I$(srctree)/$(src)/../oled_common

mcd-panel-s6e3hae_rainbow_b0-y += s6e3hae.o s6e3hae_rainbow_b0.o
mcd-panel-s6e3hae_rainbow_b0-$(CONFIG_USDM_PANEL_SELF_DISPLAY) += s6e3hae_aod.o s6e3hae_rainbow_b0_aod.o

obj-$(CONFIG_USDM_PANEL_S6E3HAE) += mcd-panel-s6e3hae_rainbow_b0.o