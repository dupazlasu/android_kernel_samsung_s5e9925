/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3hae/s6e3hae_rainbow_b0.h
 *
 * Header file for S6E3HAE RAINBOW B0 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAE_RAINBOW_B0_H__
#define __S6E3HAE_RAINBOW_B0_H__
#include "../panel.h"
#include "../panel_drv.h"

enum {
	S6E3HAE_RAINBOW_B0_HS_CLK_1362 = 0,
	S6E3HAE_RAINBOW_B0_HS_CLK_1328,
	S6E3HAE_RAINBOW_B0_HS_CLK_1368,
	MAX_S6E3HAE_RAINBOW_B0_HS_CLK
};

#define S6E3HAE_RAINBOW_B0_MAX_NIGHT_LEVEL		(102)
#define S6E3HAE_RAINBOW_B0_NUM_NIGHT_LEVEL		(S6E3HAE_RAINBOW_B0_MAX_NIGHT_LEVEL + 1)
#define S6E3HAE_RAINBOW_B0_MAX_HBM_CE_LEVEL		(3)
#define S6E3HAE_RAINBOW_B0_NUM_HBM_CE_LEVEL		(S6E3HAE_RAINBOW_B0_MAX_HBM_CE_LEVEL + 1)

#define S6E3HAE_RAINBOW_B0_SCALER_PROPERTY		("s6e3hae_rainbow_b0_scaler")

#endif /* __S6E3HAE_RAINBOW_B0_H__ */
