/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hae/s6e3hae_mdnie.h
 *
 * Header file for S6E3HAE mDNIe Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAE_MDNIE_H__
#define __S6E3HAE_MDNIE_H__

#include "oled_common_mdnie.h"

#define NR_S6E3HAE_MDNIE_REG	(3)

#define S6E3HAE_MDNIE_0_REG		(0xDF)
#define S6E3HAE_MDNIE_0_OFS		(0)
#define S6E3HAE_MDNIE_0_LEN		(224)

#define S6E3HAE_MDNIE_1_REG		(0xDE)
#define S6E3HAE_MDNIE_1_OFS		(S6E3HAE_MDNIE_0_OFS + S6E3HAE_MDNIE_0_LEN)
#define S6E3HAE_MDNIE_1_LEN		(196)

#define S6E3HAE_MDNIE_2_REG		(0xDD)
#define S6E3HAE_MDNIE_2_OFS		(S6E3HAE_MDNIE_1_OFS + S6E3HAE_MDNIE_1_LEN)
#define S6E3HAE_MDNIE_2_LEN		(21)
#define S6E3HAE_MDNIE_LEN		(S6E3HAE_MDNIE_0_LEN + S6E3HAE_MDNIE_1_LEN + S6E3HAE_MDNIE_2_LEN)

#define S6E3HAE_SCR_CR_OFS	(31)
#define S6E3HAE_SCR_WR_OFS	(49)
#define S6E3HAE_SCR_WG_OFS	(51)
#define S6E3HAE_SCR_WB_OFS	(53)
#define S6E3HAE_SCR_WHITE_LEN (2)
#define S6E3HAE_NIGHT_MODE_OFS	(S6E3HAE_SCR_CR_OFS)
#define S6E3HAE_NIGHT_MODE_LEN	(24)
#define S6E3HAE_COLOR_LENS_OFS	(S6E3HAE_SCR_CR_OFS)
#define S6E3HAE_COLOR_LENS_LEN	(24)

#define S6E3HAE_TRANS_MODE_OFS	(17)
#define S6E3HAE_TRANS_MODE_LEN	(1)

#endif /* __S6E3HAE_MDNIE_H__ */