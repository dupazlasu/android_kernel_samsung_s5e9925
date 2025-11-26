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

ifeq ($(CONFIG_MACH_R0S_EUR_OPENX), y)
mcd-panel-s6e3fac_rainbow_r0-y += s6e3fac.o s6e3fac_rainbow_r0.o
obj-$(CONFIG_USDM_PANEL_S6E3FAC) += mcd-panel-s6e3fac_rainbow_r0.o
endif

ifeq ($(CONFIG_MACH_G0S_EUR_OPENX), y)
mcd-panel-s6e3fac_rainbow_g0-y += s6e3fac.o s6e3fac_rainbow_g0.o
obj-$(CONFIG_USDM_PANEL_S6E3FAC) += mcd-panel-s6e3fac_rainbow_g0.o
endif

