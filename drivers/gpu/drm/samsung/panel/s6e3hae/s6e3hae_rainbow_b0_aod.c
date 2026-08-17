/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3hae/s6e3hae_rainbow_b0_aod.c
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../aod/aod_drv.h"

int s6e3hae_aod_self_mask_data_check(struct aod_dev_info *aod)
{
	struct panel_device *panel;
	int ret;

	if (!aod)
		return -EINVAL;

	panel = to_panel_device(aod);
	if (!check_aod_seqtbl_exist(aod, SELF_MASK_DATA_CHECK_SEQ))
		return -ENOENT;

	ret = panel_do_aod_seqtbl_by_name_nolock(aod, SELF_MASK_DATA_CHECK_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n", SELF_MASK_DATA_CHECK_SEQ);
		return -EBUSY;
	}
	if (!panel_is_dump_status_success(panel, "self_mask_checksum")) {
		panel_err("checksum(self_mask_checksum) is invalid\n");
		return -ENODATA;
	}
	return 0;
}