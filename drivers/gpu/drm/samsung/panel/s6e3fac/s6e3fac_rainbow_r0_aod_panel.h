/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac_rainbow_r0_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAC_RAINBOW_R0_AOD_PANEL_H__
#define __S6E3FAC_RAINBOW_R0_AOD_PANEL_H__

#include "s6e3fac_aod.h"
#include "s6e3fac_aod_panel.h"
#include "s6e3fac_rainbow_r0_self_mask_img.h"

/* RAINBOW_G0 */
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3fac_rainbow_r0_aod_self_mask_img_pkt,
		DSI_PKT_TYPE_WR_SR, RAINBOW_R0_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static void *s6e3fac_rainbow_r0_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(s6e3fac_aod_l1_key_enable),
	&PKTINFO(s6e3fac_aod_self_mask_sd_path),
	&DLYINFO(s6e3fac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3fac_rainbow_r0_aod_self_mask_img_pkt),
	&DLYINFO(s6e3fac_aod_self_spsram_write_delay),
	&PKTINFO(s6e3fac_aod_reset_sd_path),
	&KEYINFO(s6e3fac_aod_l1_key_disable),
};

static struct seqinfo s6e3fac_rainbow_r0_aod_seqtbl[] = {
	SEQINFO_INIT(SELF_MASK_IMG_SEQ, s6e3fac_rainbow_r0_aod_self_mask_img_cmdtbl),
	SEQINFO_INIT(SELF_MASK_ENA_SEQ, s6e3fac_aod_self_mask_ena_cmdtbl),
	SEQINFO_INIT(SELF_MASK_DIS_SEQ, s6e3fac_aod_self_mask_dis_cmdtbl),
	SEQINFO_INIT(ENTER_AOD_SEQ, s6e3fac_aod_enter_aod_cmdtbl),
	SEQINFO_INIT(EXIT_AOD_SEQ, s6e3fac_aod_exit_aod_cmdtbl),
	SEQINFO_INIT(ENABLE_PARTIAL_SCAN_SEQ, s6e3fac_aod_partial_enable_cmdtbl),
	SEQINFO_INIT(DISABLE_PARTIAL_SCAN_SEQ, s6e3fac_aod_partial_disable_cmdtbl),
	SEQINFO_INIT(SELF_MASK_CRC_SEQ, s6e3fac_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3fac_rainbow_r0_aod = {
	.name = "s6e3fac_rainbow_r0_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3fac_rainbow_r0_aod_seqtbl),
	.seqtbl = s6e3fac_rainbow_r0_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3fac_aod_maptbl),
	.maptbl = s6e3fac_aod_maptbl,
	.self_mask_en = true,
};
#endif //__S6E3FAC_RAINBOW_R0_AOD_PANEL_H__
