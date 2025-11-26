/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3fac/s6e3fac_rainbow_r0.c
 *
 * s6e3fac_rainbow_r0 Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel_debug.h"
#include "s6e3fac_rainbow_r0.h"
#include "s6e3fac_rainbow_r0_panel.h"
#include "s6e3fac_dimming.h"

__visible_for_testing int __init s6e3fac_rainbow_r0_panel_init(void)
{
	struct common_panel_info *cpi = &s6e3fac_rainbow_r0_panel_info;

	s6e3fac_init(cpi);
	register_common_panel(cpi);

	return 0;
}

__visible_for_testing void __exit s6e3fac_rainbow_r0_panel_exit(void)
{
	deregister_common_panel(&s6e3fac_rainbow_r0_panel_info);
}

module_init(s6e3fac_rainbow_r0_panel_init)
module_exit(s6e3fac_rainbow_r0_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
