/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3hae/s6e3hae_rainbow_b0.c
 *
 * s6e3hae_rainbow_b0 Driver
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
#include "s6e3hae_rainbow_b0.h"
#include "s6e3hae_rainbow_b0_panel.h"
#include "s6e3hae_dimming.h"

static struct panel_prop_enum_item s6e3hae_rainbow_b0_scaler_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_SCALER_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_SCALER_x1_78),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_SCALER_x4),
};

static int s6e3hae_rainbow_b0_scaler_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;
	int index;

	if (mres->nr_resol == 0 || mres->resol == NULL)
		return -EINVAL;

	if (props->mres_mode >= mres->nr_resol) {
		panel_err("invalid mres_mode %d, nr_resol %d\n",
				props->mres_mode, mres->nr_resol);
		return -EINVAL;
	}

	index = search_table_u32(S6E3HAE_SCALER_1440,
			ARRAY_SIZE(S6E3HAE_SCALER_1440),
			mres->resol[props->mres_mode].w);
	if (index < 0)
		return -EINVAL;

	return panel_property_set_value(prop, index);
}

static struct panel_prop_list s6e3hae_rainbow_b0_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAE_RAINBOW_B0_SCALER_PROPERTY,
			S6E3HAE_SCALER_OFF, s6e3hae_rainbow_b0_scaler_enum_items,
			s6e3hae_rainbow_b0_scaler_property_update),
};

__visible_for_testing int __init s6e3hae_rainbow_b0_panel_init(void)
{
	struct common_panel_info *cpi = &s6e3hae_rainbow_b0_panel_info;

	s6e3hae_init(cpi);
	panel_function_insert_array(s6e3hae_rainbow_b0_function_table,
			ARRAY_SIZE(s6e3hae_rainbow_b0_function_table));
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = s6e3hae_rainbow_b0_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(s6e3hae_rainbow_b0_property_array);
	register_common_panel(cpi);

	return 0;
}

__visible_for_testing void __exit s6e3hae_rainbow_b0_panel_exit(void)
{
	deregister_common_panel(&s6e3hae_rainbow_b0_panel_info);
}

module_init(s6e3hae_rainbow_b0_panel_init)
module_exit(s6e3hae_rainbow_b0_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");