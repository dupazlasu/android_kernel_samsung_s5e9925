/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hae/s6e3hae.c
 *
 * S6E3HAE Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel_kunit.h"
#include <linux/bits.h>
#include "../panel.h"
#include "../panel_function.h"
#include "s6e3hae.h"
#include "s6e3hae_dimming.h"
#ifdef CONFIG_USDM_PANEL_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../dpui.h"
#include "../util.h"
#include "oled_common.h"
#include "oled_property.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

unsigned int s6e3hae_gamma_bits[S6E3HAE_NR_TP] = {
	12, 12, 11, 11, 11, 11, 11, 11,
	11, 11, 11, 11, 11, 12
};

unsigned int s6e3hae_glut_bits[S6E3HAE_NR_GLUT_POINT] = {
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
};

int get_s6e3hae_96hs_mode(int vrr)
{
	int ret;

	switch (vrr) {
	case S6E3HAE_VRR_96HS:
	case S6E3HAE_VRR_48HS_96HS_TE_HW_SKIP_1:
		ret = S6E3HAE_96HS_MODE_ON;
		break;
	case S6E3HAE_VRR_60NS:
	case S6E3HAE_VRR_48NS:
	case S6E3HAE_VRR_120HS:
	case S6E3HAE_VRR_60HS_120HS_TE_HW_SKIP_1:
	case S6E3HAE_VRR_30HS_120HS_TE_HW_SKIP_3:
	case S6E3HAE_VRR_24HS_120HS_TE_HW_SKIP_4:
	case S6E3HAE_VRR_10HS_120HS_TE_HW_SKIP_11:
		ret = S6E3HAE_96HS_MODE_OFF;
		break;
	default:
		//error
		panel_err("got invalid idx %d\n", vrr);
		ret = S6E3HAE_96HS_MODE_OFF;
		break;
	}

	return ret;
}

int find_s6e3hae_vrr(struct panel_vrr *vrr)
{
	int i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3HAE_VRR_FPS); i++)
		if (vrr->fps == S6E3HAE_VRR_FPS[i][S6E3HAE_VRR_KEY_REFRESH_RATE] &&
			vrr->mode == S6E3HAE_VRR_FPS[i][S6E3HAE_VRR_KEY_REFRESH_MODE] &&
			vrr->te_sw_skip_count == S6E3HAE_VRR_FPS[i][S6E3HAE_VRR_KEY_TE_SW_SKIP_COUNT] &&
			vrr->te_hw_skip_count == S6E3HAE_VRR_FPS[i][S6E3HAE_VRR_KEY_TE_HW_SKIP_COUNT])
			return i;

	return -EINVAL;
}

int getidx_s6e3hae_lfd_frame_idx(int fps, int mode)
{
	int i = 0, start = 0, end = 0;

	if (mode == VRR_HS_MODE) {
		start = S6E3HAE_VRR_LFD_FRAME_IDX_HS_BEGIN;
		end = S6E3HAE_VRR_LFD_FRAME_IDX_HS_END;
	} else if (mode == VRR_NORMAL_MODE) {
		start = S6E3HAE_VRR_LFD_FRAME_IDX_NS_BEGIN;
		end = S6E3HAE_VRR_LFD_FRAME_IDX_NS_END;
	}
	for (i = start; i <= end; i++)
		if (fps >= S6E3HAE_VRR_LFD_FRAME_IDX_VAL[i])
			return i;

	return -EINVAL;
}

int get_s6e3hae_lfd_min_freq(u32 scalability, u32 vrr_fps_index)
{
	if (scalability > S6E3HAE_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAE_VRR_LFD_SCALABILITY_MAX;
	}
	if (vrr_fps_index >= MAX_S6E3HAE_VRR) {
		panel_err("invalid vrr_fps_index %d\n", vrr_fps_index);
		return -EINVAL;
	}
	return S6E3HAE_VRR_LFD_MIN_FREQ[scalability][vrr_fps_index];
}

int get_s6e3hae_lpm_lfd_min_freq(u32 scalability)
{
	if (scalability > S6E3HAE_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAE_VRR_LFD_SCALABILITY_MAX;
	}
	return S6E3HAE_LPM_LFD_MIN_FREQ[scalability];
}

int s6e3hae_get_vrr_lfd_min_div_count(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct panel_vrr *vrr;
	int index = 0, ret;
	int vrr_fps, vrr_mode;
	u32 lfd_min_freq = 0;
	u32 lfd_min_div_count = 0;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	index = find_s6e3hae_vrr(vrr);
	if (index < 0) {
		panel_err("vrr(%d %d) not found\n",
				vrr_fps, vrr_mode);
		return -EINVAL;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	ret = get_s6e3hae_lfd_min_freq(vrr_lfd_config->scalability, index);
	if (ret < 0) {
		panel_err("failed to get lfd_min_freq(%d)\n", ret);
		return -EINVAL;
	}
	lfd_min_freq = ret;

	if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH) {
		lfd_min_div_count = vrr_div_count;
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH_UPTO_SCAN_FREQ) {
		lfd_min_div_count = TE_SKIP_TO_DIV(0, 0);
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_LOW ||
			vrr_lfd_config->min == 0 ||
			vrr_div_count == 0) {
		lfd_min_div_count = disp_div_round(vrr_fps, lfd_min_freq);
	} else {
		lfd_min_freq = max(lfd_min_freq,
				min(disp_div_round(vrr_fps, vrr_div_count), vrr_lfd_config->min));
		lfd_min_div_count = (lfd_min_freq == 0) ?
			MIN_S6E3HAE_FPS_DIV_COUNT : disp_div_round(vrr_fps, lfd_min_freq);
	}

	panel_dbg("vrr(%d %d), div(%d), lfd(fix:%d scale:%d min:%d max:%d), div_count(%d)\n",
			vrr_fps, vrr_mode, vrr_div_count,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max,
			lfd_min_div_count);

	return lfd_min_div_count;
}

int s6e3hae_get_vrr_lfd_max_div_count(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct panel_vrr *vrr;
	int index = 0, ret;
	int vrr_fps, vrr_mode;
	u32 lfd_min_freq = 0;
	u32 lfd_max_freq = 0;
	u32 lfd_max_div_count = 0;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	index = find_s6e3hae_vrr(vrr);
	if (index < 0) {
		panel_err("vrr(%d %d) not found\n",
				vrr_fps, vrr_mode);
		return -EINVAL;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	ret = get_s6e3hae_lfd_min_freq(vrr_lfd_config->scalability, index);
	if (ret < 0) {
		panel_err("failed to get lfd_min_freq(%d)\n", ret);
		return -EINVAL;
	}

	lfd_min_freq = ret;
	if (vrr_lfd_config->fix == VRR_LFD_FREQ_LOW) {
		lfd_max_div_count = disp_div_round((u32)vrr_fps, lfd_min_freq);
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH_UPTO_SCAN_FREQ) {
		lfd_max_div_count = TE_SKIP_TO_DIV(0, 0);
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH ||
		vrr_lfd_config->max == 0 ||
		vrr_div_count == 0) {
		lfd_max_div_count = vrr_div_count;
	} else {
		lfd_max_freq = max(lfd_min_freq,
				min(disp_div_round(vrr_fps, vrr_div_count), vrr_lfd_config->max));
		lfd_max_div_count = (lfd_max_freq == 0) ?
			MIN_S6E3HAE_FPS_DIV_COUNT : disp_div_round(vrr_fps, lfd_max_freq);
	}

	panel_dbg("vrr(%d %d), div(%d), lfd(fix:%d scale:%d min:%d max:%d), div_count(%d)\n",
			vrr_fps, vrr_mode, vrr_div_count,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max,
			lfd_max_div_count);

	return lfd_max_div_count;
}

int s6e3hae_get_vrr_lfd_min_freq(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	int vrr_fps, div_count;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	div_count = s6e3hae_get_vrr_lfd_min_div_count(panel_data);
	if (div_count <= 0)
		return -EINVAL;

	return (int)disp_div_round(vrr_fps, div_count);
}

int s6e3hae_get_vrr_lfd_max_freq(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	int vrr_fps, div_count;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	div_count = s6e3hae_get_vrr_lfd_max_div_count(panel_data);
	if (div_count <= 0)
		return -EINVAL;

	return (int)disp_div_round(vrr_fps, div_count);
}
#ifdef CONFIG_USDM_PANEL_DIMMING
__visible_for_testing int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if (likely(brt_tbl->brt_to_step)) {
			panel_info("we use static step table\n");
			return ret;
		} else {
			panel_err("invalid parameter, all table is NULL\n");
			return -EINVAL;
		}
	}

	brt_tbl->sz_brt_to_step = 0;
	for (i = 0; i < brt_tbl->sz_step_cnt; i++)
		brt_tbl->sz_brt_to_step += brt_tbl->step_cnt[i];

	brt_tbl->brt_to_step =
		(u32 *)kmalloc(brt_tbl->sz_brt_to_step * sizeof(u32), GFP_KERNEL);

	if (unlikely(!brt_tbl->brt_to_step)) {
		panel_err("alloc fail\n");
		return -EINVAL;
	}
	brt_tbl->brt_to_step[0] = brt_tbl->brt[0];
	i = 1;
	while (i < brt_tbl->sz_brt_to_step) {
		for (k = 1; k < brt_tbl->sz_brt; k++) {
			for (j = 1; j <= brt_tbl->step_cnt[k]; j++, i++) {
				brt_tbl->brt_to_step[i] = disp_interpolation64(brt_tbl->brt[k - 1] * disp_pow(10, 2),
					brt_tbl->brt[k] * disp_pow(10, 2), j, brt_tbl->step_cnt[k]);
				brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				brt_tbl->brt_to_step[i] = disp_div64(brt_tbl->brt_to_step[i], disp_pow(10, 2));
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i]) {
					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				}
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
}
#endif /* CONFIG_USDM_PANEL_DIMMING */

#ifdef CONFIG_USDM_PANEL_LPM
#ifdef CONFIG_USDM_PANEL_AOD_BL
__visible_for_testing int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_bl = &panel->panel_bl;

	if (unlikely(!panel->panel_data.panel_dim_info[id])) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}

	memcpy(&panel_bl->subdev[id].brt_tbl,
			panel->panel_data.panel_dim_info[id]->brt_tbl,
			sizeof(struct brightness_table));

	return 0;
}
#endif
#endif

__visible_for_testing int s6e3hae_maptbl_init_lpm_brt(struct maptbl *tbl)
{
#ifdef CONFIG_USDM_PANEL_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return oled_maptbl_init_default(tbl);
#endif
}

__visible_for_testing int s6e3hae_maptbl_init_gamma_mode2_brt(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;

	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_DISP].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

#ifdef CONFIG_USDM_PANEL_HMD
__visible_for_testing int s6e3hae_maptbl_init_gamma_mode2_hmd_brt(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;

	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_HMD];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_HMD].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}
#endif

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
int s6e3hae_getidx_vddm_table(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_properties *props;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	props = &panel->panel_data.props;

	if (props->gct_vddm >= maptbl_get_countof_row(tbl))
		return -EINVAL;

	panel_info("vddm %d\n", props->gct_vddm);

	return maptbl_index(tbl, 0, props->gct_vddm, 0);
}

int s6e3hae_getidx_gram_img_pattern_table(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_properties *props;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	props = &panel->panel_data.props;

	panel_info("gram img %d\n", props->gct_pattern);
	props->gct_valid_chksum[0] = S6E3HAE_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3HAE_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3HAE_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3HAE_GRAM_CHECKSUM_VALID_2;

	if (props->gct_pattern >= maptbl_get_countof_row(tbl))
		return -EINVAL;

	return maptbl_index(tbl, 0, props->gct_pattern, 0);
}

/* TODO: remove this function */
__visible_for_testing int s6e3hae_maptbl_init_gram_img_pattern(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	props->gct_valid_chksum[0] = S6E3HAE_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3HAE_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3HAE_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3HAE_GRAM_CHECKSUM_VALID_2;

	return oled_maptbl_init_default(tbl);
}
#endif

#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
int s6e3hae_getidx_brightdot_aor_table(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_properties *props;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	props = &panel->panel_data.props;

	if (props->brightdot_test_enable >= maptbl_get_countof_row(tbl))
		return -EINVAL;

	return maptbl_index(tbl, 0, props->brightdot_test_enable, 0);
}

bool s6e3hae_cond_is_brightdot_enabled(struct panel_device *panel)
{
	if (!panel)
		return false;

	if (panel->panel_data.props.brightdot_test_enable != 0) {
		panel_info(" true\n");
		return true;
	}

	return false;
}
#endif

__visible_for_testing int gamma_int_array_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR])
{
	unsigned int i, c;
	int max_value;

	if (!src || !dst || !offset)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3hae_gamma_bits); i++) {
		max_value = GENMASK(s6e3hae_gamma_bits[i] - 1, 0);
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), max_value);
	}

	return 0;
}

__visible_for_testing int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src)
{
	unsigned int i, mask, upper_mask, lower_mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3hae_gamma_bits); i++) {
		mask = GENMASK(s6e3hae_gamma_bits[i] - 1, 0);
		upper_mask = (mask >> 8) & 0xFF;
		lower_mask = mask & 0xFF;

		dst[i][RED] = (((src[i * 5 + 0] & upper_mask) << 8) | (src[i * 5 + 2] & lower_mask));
		dst[i][GREEN] = ((((src[i * 5 + 1] >> 4) & upper_mask) << 8) | (src[i * 5 + 3] & lower_mask));
		dst[i][BLUE] = (((src[i * 5 + 1] & upper_mask) << 8) | (src[i * 5 + 4] & lower_mask));
	}

	return 0;
}

__visible_for_testing int gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR])
{
	unsigned int i, mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3hae_gamma_bits); i++) {
		mask = GENMASK(s6e3hae_gamma_bits[i] - 1, 0);
		dst[i * 5 + 0] = (src[i][RED] >> 8) & (mask >> 8);
		dst[i * 5 + 1] = ((src[i][GREEN] >> 8) & (mask >> 8)) << 4 |
						 ((src[i][BLUE] >> 8) & (mask >> 8));
		dst[i * 5 + 2] = src[i][RED] & (mask & 0xFF);
		dst[i * 5 + 3] = src[i][GREEN] & (mask & 0xFF);
		dst[i * 5 + 4] = src[i][BLUE] & (mask & 0xFF);
	}

	return 0;
}

__visible_for_testing int gamma_byte_array_sum(u8 *dst, u8 *src,
		s32 (*offset)[MAX_COLOR])
{
	s32 gamma_org[S6E3HAE_NR_TP][MAX_COLOR];
	s32 gamma_new[S6E3HAE_NR_TP][MAX_COLOR];

	if (!dst || !src || !offset)
		return -EINVAL;

	gamma_ctoi(gamma_org, src);
	gamma_int_array_sum(gamma_new, gamma_org, offset);
	gamma_itoc(dst, gamma_new);

	return 0;
}

__visible_for_testing struct dimming_color_offset *get_dimming_data(struct panel_device *panel, int idx)
{
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	struct dimming_color_offset *dimming_data;

	if (!panel)
		return ERR_PTR(-EINVAL);

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (!panel_dim_info)
		return ERR_PTR(-EINVAL);

	if (!panel_dim_info->dimming_data || panel_dim_info->nr_dimming_data < 1)
		return ERR_PTR(-ENODATA);

	if (idx < 0 || idx >= panel_dim_info->nr_dimming_data)
		return ERR_PTR(-ERANGE);

	dimming_data = panel_dim_info->dimming_data;
	return (dimming_data + idx);
}

__visible_for_testing int s6e3hae_maptbl_init_gamma_mtp(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct dimming_color_offset *dim_data;
	u8 gamma_data[S6E3HAE_GAMMA_MTP_WRITE_LEN];
	int i, ret, len, ofs = 0;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("invalid maptbl\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	for (i = 0; i < S6E3HAE_GAMMA_MTP_WRITE_SET_COUNT; i++) {
		len = get_panel_resource_size(panel, S6E3HAE_GAMMA_MTP_INIT_SRC[i]);
		if (len < 0)
			return len;

		if (maptbl_get_sizeof_maptbl(tbl) < ofs + len) {
			//out of range
			return -EINVAL;
		}
		ret = panel_resource_copy(panel, gamma_data + ofs, S6E3HAE_GAMMA_MTP_INIT_SRC[i]);
		if (ret < 0)
			return ret;

		panel_dbg("copy resource %s[%d]\n", S6E3HAE_GAMMA_MTP_INIT_SRC[i], i);

		dim_data = get_dimming_data(panel, i);
		if (!IS_ERR_OR_NULL(dim_data) && dim_data->rgb_color_offset) {
			gamma_byte_array_sum(gamma_data + ofs, gamma_data + ofs, dim_data->rgb_color_offset);
			panel_info("update gamma %s with offset[%d]\n", S6E3HAE_GAMMA_MTP_INIT_SRC[i], i);
		}
		ofs += len;
	}

	memcpy(tbl->arr, gamma_data, maptbl_get_sizeof_maptbl(tbl));

	return 0;
}
#ifdef CONFIG_USDM_PANEL_MAFPC
__visible_for_testing void s6e3hae_maptbl_copy_mafpc_enable(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mafpc_device *mafpc;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;

	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	panel_info("MCD:ABC:enabled: %x, written: %x\n", mafpc->enable, mafpc->written);

	if (!mafpc->enable) {
		dst[0] = 0;
		goto err_enable;
	}

	if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) &&
		(mafpc->written & MAFPC_UPDATED_TO_DEV)) {

		dst[0] = S6E3HAE_MAFPC_ENABLE;

		if (mafpc->written & MAFPC_UPDATED_FROM_SVC)
			memcpy(&dst[S6E3HAE_MAFPC_CTRL_CMD_OFFSET], mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);
	}

err_enable:
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, S6E3HAE_MAFPC_CTRL_CMD_OFFSET + mafpc->ctrl_cmd_len, false);
	return;
}

__visible_for_testing void s6e3hae_maptbl_copy_mafpc_ctrl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mafpc_device *mafpc;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;

	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (mafpc->enable) {
		if (mafpc->written)
			memcpy(dst, mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);

		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, dst, mafpc->ctrl_cmd_len, false);
	}

	return;
}

__visible_for_testing int get_mafpc_scale_index(struct mafpc_device *mafpc, struct panel_device *panel)
{
	int ret = 0;
	int br_index, index = 0;
	struct panel_bl_device *panel_bl;

	panel_bl = &panel->panel_bl;
	if (!panel_bl) {
		panel_err("panel_bl is null\n");
		goto err_get_scale;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		goto err_get_scale;
	}

	br_index = panel_bl->props.brightness;
	if (br_index >= mafpc->scale_map_br_tbl_len)
		br_index = mafpc->scale_map_br_tbl_len - 1;

	index = mafpc->scale_map_br_tbl[br_index];
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", br_index);
		goto err_get_scale;
	}
	return index;

err_get_scale:
	return ret;
}

__visible_for_testing void s6e3hae_maptbl_copy_mafpc_scale(struct maptbl *tbl, u8 *dst)
{
	int row = 0;
	int index = 0;

	struct mafpc_device *mafpc;
	struct panel_device *panel;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;

	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		if (!mafpc->scale_buf)
			panel_err("MCD:ABC: scale_buf is null\n");
		if (!mafpc->scale_map_br_tbl)
			panel_err("MCD:ABC: scale_map_br_tbl is null\n");
		return;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		return;
	}

	if (index >= S6E3HAE_MAFPC_SCALE_MAX)
		index = S6E3HAE_MAFPC_SCALE_MAX - 1;

	row = index * 3;
	memcpy(dst, mafpc->scale_buf + row, 3);

	panel_info("idx: %d, %x:%x:%x\n",
			index, dst[0], dst[1], dst[2]);

	return;
}
#endif /* CONFIG_USDM_PANEL_MAFPC */

int s6e3hae_getidx_lpm_fps_table(struct maptbl *tbl)
{
	int row = S6E3HAE_LPM_LFD_1HZ, lpm_lfd_min_freq;
	struct panel_device *panel;
	struct panel_properties *props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;

	panel = (struct panel_device *)tbl->pdata;
	props = &panel->panel_data.props;

	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_LPM];
	vrr_lfd_status->lfd_max_freq = 30;
	vrr_lfd_status->lfd_max_freq_div = 1;
	vrr_lfd_status->lfd_min_freq = 1;
	vrr_lfd_status->lfd_min_freq_div = 30;

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_LPM];
	if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH ||
		vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH_UPTO_SCAN_FREQ) {
		row = S6E3HAE_LPM_LFD_30HZ;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		panel_info("lpm_fps %dhz (row:%d)\n",
				(row == S6E3HAE_LPM_LFD_1HZ) ? 1 : 30, row);
		return maptbl_index(tbl, 0, row, 0);
	}

	lpm_lfd_min_freq =
		get_s6e3hae_lpm_lfd_min_freq(vrr_lfd_config->scalability);
	if (lpm_lfd_min_freq <= 0 || lpm_lfd_min_freq > 1) {
		row = S6E3HAE_LPM_LFD_30HZ;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		panel_info("lpm_fps %dhz (row:%d)\n",
				(row == S6E3HAE_LPM_LFD_1HZ) ? 1 : 30, row);
		return maptbl_index(tbl, 0, row, 0);
	}

#ifdef CONFIG_MCD_PANEL_LPM
	switch (props->lpm_fps) {
	case S6E3HAE_LPM_LFD_1HZ:
		row = props->lpm_fps;
		vrr_lfd_status->lfd_min_freq = 1;
		vrr_lfd_status->lfd_min_freq_div = 30;
		break;
	case S6E3HAE_LPM_LFD_30HZ:
		row = props->lpm_fps;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		break;
	default:
		panel_err("invalid lpm_fps %d\n", props->lpm_fps);
		break;
	}
	panel_info("lpm_fps %dhz(row:%d)\n",
			(row == S6E3HAE_LPM_LFD_1HZ) ? 1 : 30, row);
#endif

	return maptbl_index(tbl, 0, row, 0);
}

int s6e3hae_getidx_vrr_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int vrr_mode, row = 0, layer = 0;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	row = (vrr_mode == S6E3HAE_VRR_MODE_HS) ?
		S6E3HAE_VRR_MODE_HS : S6E3HAE_VRR_MODE_NS;
	panel_dbg("vrr_mode:%d(%s)\n", vrr_mode,
			vrr_mode == S6E3HAE_VRR_MODE_HS ? "HS" : "NM");

	return maptbl_index(tbl, layer, row, 0);
}

int s6e3hae_getidx_lfd_frame_insertion_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_config *vrr_lfd_config;
	int vrr_mode;
	int lfd_max_freq, lfd_max_index;
	int lfd_min_freq, lfd_min_index;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	lfd_max_freq = s6e3hae_get_vrr_lfd_max_freq(panel_data);
	if (lfd_max_freq < 0) {
		panel_err("failed to get s6e3hae_get_vrr_lfd_max_freq\n");
		return -EINVAL;
	}

	lfd_min_freq = s6e3hae_get_vrr_lfd_min_freq(panel_data);
	if (lfd_min_freq < 0) {
		panel_err("failed to get s6e3hae_get_vrr_lfd_min_freq\n");
		return -EINVAL;
	}

	lfd_max_index = getidx_s6e3hae_lfd_frame_idx(lfd_max_freq, vrr_mode);
	if (lfd_max_index < 0) {
		panel_err("failed to get lfd_max_index(lfd_max_freq:%d vrr_mode:%d)\n",
				lfd_max_freq, vrr_mode);
		return -EINVAL;
	}

	lfd_min_index = getidx_s6e3hae_lfd_frame_idx(lfd_min_freq, vrr_mode);
	if (lfd_min_index < 0) {
		panel_err("failed to get lfd_min_index(lfd_min_freq:%d vrr_mode:%d)\n",
				lfd_min_freq, vrr_mode);
		return -EINVAL;
	}

	panel_dbg("lfd_max_freq %d%s(%d) lfd_min_freq %d%s(%d)\n",
			lfd_max_freq, vrr_mode == S6E3HAE_VRR_MODE_HS ? "HS" : "NM", lfd_max_index,
			lfd_min_freq, vrr_mode == S6E3HAE_VRR_MODE_HS ? "HS" : "NM", lfd_min_index);

	return maptbl_index(tbl, lfd_max_index, lfd_min_index, 0);
}

void s6e3hae_maptbl_copy_lfd_min(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	u32 vrr_div_count, value;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	if (vrr_div_count < MIN_S6E3HAE_FPS_DIV_COUNT ||
		vrr_div_count > MAX_S6E3HAE_FPS_DIV_COUNT) {
		panel_err("out of range vrr(%d %d) vrr_div_count(%d)\n",
				vrr_fps, vrr_mode, vrr_div_count);
		return;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	vrr_div_count = s6e3hae_get_vrr_lfd_min_div_count(panel_data);
	if (vrr_div_count <= 0) {
		panel_err("failed to get vrr(%d %d) div count\n",
				vrr_fps, vrr_mode);
		return;
	}

	/* update lfd_min status */
	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL];
	vrr_lfd_status->lfd_min_freq_div = vrr_div_count;
	vrr_lfd_status->lfd_min_freq =
		disp_div_round(vrr_fps, vrr_div_count);

	panel_dbg("vrr(%d %d) lfd(fix:%d scale:%d min:%d max:%d) --> lfd_min(1/%d)\n",
			vrr_fps, vrr_mode,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, vrr_div_count);

	/* change modulation count to skip frame count */
	value = (u32)(vrr_div_count - MIN_VRR_DIV_COUNT) << 1;
	dst[0] = (value >> 8) & 0xFF;
	dst[1] = value & 0xFF;
}

void s6e3hae_maptbl_copy_lfd_max(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	u32 vrr_div_count, value;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	if (vrr_div_count < MIN_S6E3HAE_FPS_DIV_COUNT ||
		vrr_div_count > MAX_S6E3HAE_FPS_DIV_COUNT) {
		panel_err("out of range vrr(%d %d) vrr_div_count(%d)\n",
				vrr_fps, vrr_mode, vrr_div_count);
		return;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	vrr_div_count = s6e3hae_get_vrr_lfd_max_div_count(panel_data);
	if (vrr_div_count <= 0) {
		panel_err("failed to get vrr(%d %d) div count\n",
				vrr_fps, vrr_mode);
		return;
	}

	/* update lfd_max status */
	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL];
	vrr_lfd_status->lfd_max_freq_div = vrr_div_count;
	vrr_lfd_status->lfd_max_freq =
		disp_div_round(vrr_fps, vrr_div_count);

	panel_dbg("vrr(%d %d) lfd(fix:%d scale:%d min:%d max:%d) --> lfd_max(1/%d)\n",
			vrr_fps, vrr_mode,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, vrr_div_count);

	/* change modulation count to skip frame count */
	value = (u32)(vrr_div_count - MIN_VRR_DIV_COUNT) << 1;
	dst[0] = (value >> 8) & 0xFF;
	dst[1] = value & 0xFF;
}

int s6e3hae_getidx_ffc_table(struct maptbl *tbl)
{
	int idx;
	u8 ddi_rev;
	u32 dsi_clk;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	dsi_clk = panel_data->props.dsi_freq;
	ddi_rev = (panel_data->id[2] & 0x30) >> 4;
	panel_info("id[3]: %d -> ddi_rev: %d, dsi_clk: %dkz\n",
		panel_data->id[2], ddi_rev, dsi_clk);

	if (ddi_rev >= MAX_DDI_REV) {
		panel_err("invalid ddi rev: %d\n", ddi_rev);
		ddi_rev = DDI_REV_EVT1;
	}

	if (ddi_rev == DDI_REV_EVT1)
		panel_err("Need to check.. EVT1's ffc was not tunned. check op manual\n");

	switch (dsi_clk) {
	case 1362000:
		idx = HS_CLK_1362;
		break;
	case 1328000:
		idx = HS_CLK_1328;
		break;
	case 1368000:
		idx = HS_CLK_1368;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		BUG();
	}
	return maptbl_index(tbl, ddi_rev, idx, 0);
}int s6e3hae_do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len)
{
	int ret, state;
	struct dim_flash_result *result;
	char read_buf[16] = { 0, };

	result = (struct dim_flash_result *)data;

	if (!panel)
		return -EINVAL;

	if (!result)
		return -ENODATA;

	if (atomic_cmpxchg(&result->running, 0, 1) != 0) {
		panel_info("already running\n");
		return -EBUSY;
	}

	memset(result->result, 0, ARRAY_SIZE(result->result));
	result->exist = 0;
	result->state = state = GAMMA_FLASH_PROGRESS;

	ret = panel_resource_copy(panel, read_buf, "flash_loaded");
	if (ret < 0) {
		panel_err("flash_loaded copy failed\n");
		state = GAMMA_FLASH_ERROR_READ_FAIL;
		goto out;
	}

	result->exist = 1;
	state = panel_is_dump_status_success(panel, "flash_loaded") ?
		GAMMA_FLASH_SUCCESS : GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;

out:
	snprintf(result->result, ARRAY_SIZE(result->result), "1\n%d %02X%02X%02X%02X",
		state, read_buf[0], read_buf[1], 0x00, 0x00);

	result->state = state;
	atomic_xchg(&result->running, 0);

	return ret;
}

int s6e3hae_get_octa_id(struct panel_device *panel, void *buf)
{
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	int len = 0;
	bool cell_id_exist = true;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_resource_copy(panel, octa_id, "octa_id");

	site = (octa_id[0] >> 4) & 0x0F;
	rework = octa_id[0] & 0x0F;
	poc = octa_id[1] & 0x0F;

	panel_dbg("site (%d), rework (%d), poc (%d)\n",
			site, rework, poc);

	panel_dbg("<CELL ID>\n");
	for (i = 0; i < 16; i++) {
		cell_id[i] = isalnum(octa_id[i + 4]) ? octa_id[i + 4] : '\0';
		panel_dbg("%x -> %c\n", octa_id[i + 4], cell_id[i]);
		if (cell_id[i] == '\0') {
			cell_id_exist = false;
			break;
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%d%d%d%02x%02x",
			site, rework, poc, octa_id[2], octa_id[3]);
	if (cell_id_exist) {
		for (i = 0; i < 16; i++)
			len += snprintf(buf + len, PAGE_SIZE - len, "%c", cell_id[i]);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return 0;
}

int s6e3hae_get_cell_id(struct panel_device *panel, void *buf)
{
	u8 date[PANEL_DATE_LEN] = { 0, }, coordinate[4] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_resource_copy(panel, date, "date");
	panel_resource_copy(panel, coordinate, "coordinate");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		date[0], date[1], date[2], date[3], date[4], date[5], date[6],
		coordinate[0], coordinate[1], coordinate[2], coordinate[3]);
	return 0;
}

int s6e3hae_get_manufacture_code(struct panel_device *panel, void *buf)
{
	u8 code[5] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_resource_copy(panel, code, "code");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X\n",
		code[0], code[1], code[2], code[3], code[4]);

	return 0;
}

int s6e3hae_get_manufacture_date(struct panel_device *panel, void *buf)
{
	u16 year;
	u8 month, day, hour, min, date[PANEL_DATE_LEN] = { 0, };
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	ret = panel_resource_copy(panel, date, "date");
	if (ret < 0) {
		panel_err("failed to copy resources\n");
		return -ENODATA;
	}

	year = ((date[0] & 0xF0) >> 4) + 2011;
	month = date[0] & 0xF;
	day = date[1] & 0x1F;
	hour = date[2] & 0x1F;
	min = date[3] & 0x3F;

	snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d:%d\n",
			year, month, day, hour, min);

	return 0;
}

__visible_for_testing u32 s6e3hae_get_ddi_rev(struct panel_device *panel)
{
	struct panel_info *panel_data;
	u32 ddi_rev;

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;
	ddi_rev = (panel_data->id[2] & 0x30) >> 4;

	if (ddi_rev >= MAX_DDI_REV) {
		panel_err("invalid ddi_rev range %u", ddi_rev);
		return 0;
	}
	return ddi_rev;
}

bool s6e3hae_cond_is_wait_vsync_needed(struct panel_device *panel)
{
	struct panel_properties *props;

	if (!panel)
		return false;

	props = &panel->panel_data.props;
	if (props->vrr_origin_mode != props->vrr_mode) {
		panel_info("true(vrr mode changed)\n");
		return true;
	}

	if (!!(props->vrr_origin_fps % 48) != !!(props->vrr_fps % 48)) {
		panel_info("true(adaptive vrr changed)\n");
		return true;
	}

	return false;
}

bool s6e3hae_cond_is_support_lfd(struct panel_device *panel)
{
	if (!panel)
		return false;

	return ((panel->panel_data.id[2] & 0xFF) >= 0x90) ? true : false;
}

bool s6e3hae_cond_is_support_lpm_lfd(struct panel_device *panel)
{
	if (!panel)
		return false;

	return ((panel->panel_data.id[2] & 0xFF) >= 0x93) ? true : false;
}

bool s6e3hae_cond_is_display_on(struct panel_device *panel)
{
	if (!panel)
		return false;

	return panel->state.disp_on == PANEL_DISPLAY_ON;
}

/* we should run with 96HS mode, when the display's fps is 96hz */
bool s6e3hae_cond_is_96hs_based_fps(struct panel_device *panel)
{
	int refresh_rate, refresh_mode;

	if (!panel)
		return false;

	refresh_rate = get_panel_refresh_rate(panel);
	refresh_mode = get_panel_refresh_mode(panel);
	if (refresh_mode == VRR_HS_MODE && refresh_rate == 96)
		return true;

	return false;
}static int s6e3hae_smooth_dim_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;

	return panel_property_set_value(prop,
			(props->vrr_idx != props->vrr_origin_idx ||
			 panel->state.disp_on == PANEL_DISPLAY_OFF) ?
			S6E3HAE_SMOOTH_DIM_NOT_USE : S6E3HAE_SMOOTH_DIM_USE);
}

static struct panel_prop_enum_item s6e3hae_smooth_dim_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_SMOOTH_DIM_USE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_SMOOTH_DIM_NOT_USE),
};

static int s6e3hae_ddi_rev_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;

	return panel_property_set_value(prop, s6e3hae_get_ddi_rev(panel));
}

static struct panel_prop_enum_item s6e3hae_ddi_rev_enum_items[MAX_DDI_REV] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(DDI_REV_EVT0),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(DDI_REV_EVT0_OSC),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(DDI_REV_EVT1),
};

static int s6e3hae_acl_opr_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	unsigned int acl_opr = panel_bl_get_acl_opr(panel_bl);

	return panel_property_set_value(prop,
			min(acl_opr, (u32)S6E3HAE_ACL_OPR_3));
}

static struct panel_prop_enum_item s6e3hae_acl_opr_enum_items[MAX_S6E3HAE_ACL_OPR] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_ACL_OPR_0),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_ACL_OPR_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_ACL_OPR_2),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_ACL_OPR_3),
};

static int s6e3hae_vrr_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;

	return panel_property_set_value(prop,
			find_s6e3hae_vrr(get_panel_vrr(panel)));
}

static struct panel_prop_enum_item s6e3hae_vrr_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_60NS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_48NS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_120HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_60HS_120HS_TE_HW_SKIP_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_30HS_120HS_TE_HW_SKIP_3),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_24HS_120HS_TE_HW_SKIP_4),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_10HS_120HS_TE_HW_SKIP_11),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_96HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAE_VRR_48HS_96HS_TE_HW_SKIP_1),
};

static int s6e3hae_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step_by_subdev_id(panel_bl,
				PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness));
}

static int s6e3hae_lpm_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel->panel_data.props.lpm_brightness =
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;

	return panel_property_set_value(prop,
			get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness));
}

static struct panel_prop_list s6e3hae_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAE_SMOOTH_DIM_PROPERTY,
			S6E3HAE_SMOOTH_DIM_USE, s6e3hae_smooth_dim_enum_items,
			s6e3hae_smooth_dim_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAE_DDI_REV_PROPERTY,
			DDI_REV_EVT0, s6e3hae_ddi_rev_enum_items,
			s6e3hae_ddi_rev_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAE_ACL_OPR_PROPERTY,
			S6E3HAE_ACL_OPR_0, s6e3hae_acl_opr_enum_items,
			s6e3hae_acl_opr_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAE_VRR_PROPERTY,
			S6E3HAE_VRR_60NS, s6e3hae_vrr_enum_items,
			s6e3hae_vrr_property_update),
	/* range property */
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_NRM_BR_INDEX_PROPERTY,
			0, 0, S6E3HAE_RAINBOW_B0_TOTAL_NR_LUMINANCE,
			s6e3hae_br_index_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_LPM_BR_INDEX_PROPERTY,
			0, 0, S6E3HAE_RAINBOW_B0_AOD_NR_LUMINANCE,
			s6e3hae_lpm_br_index_property_update),
};

struct pnobj_func s6e3hae_function_table[MAX_S6E3HAE_FUNCTION] = {
	[S6E3HAE_MAPTBL_INIT_GAMMA_MODE2_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_INIT_GAMMA_MODE2_BRT, s6e3hae_maptbl_init_gamma_mode2_brt),
	[S6E3HAE_MAPTBL_INIT_LPM_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_INIT_LPM_BRT, s6e3hae_maptbl_init_lpm_brt),
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[S6E3HAE_MAPTBL_INIT_GRAM_IMG_PATTERN] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_INIT_GRAM_IMG_PATTERN, s6e3hae_maptbl_init_gram_img_pattern),
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	[S6E3HAE_MAPTBL_COPY_MAFPC_ENABLE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_COPY_MAFPC_ENABLE, s6e3hae_maptbl_copy_mafpc_enable),
	[S6E3HAE_MAPTBL_COPY_MAFPC_CTRL] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_COPY_MAFPC_CTRL, s6e3hae_maptbl_copy_mafpc_ctrl),
	[S6E3HAE_MAPTBL_COPY_MAFPC_SCALE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_COPY_MAFPC_SCALE, s6e3hae_maptbl_copy_mafpc_scale),
#endif
#ifdef CONFIG_USDM_PANEL_HMD
	[S6E3HAE_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT, s6e3hae_maptbl_init_gamma_mode2_hmd_brt),
#endif
	[S6E3HAE_MAPTBL_INIT_GAMMA_MTP] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_MAPTBL_INIT_GAMMA_MTP, s6e3hae_maptbl_init_gamma_mtp),
	[S6E3HAE_COND_IS_WAIT_VSYNC_NEEDED] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COND_IS_WAIT_VSYNC_NEEDED, s6e3hae_cond_is_wait_vsync_needed),
	[S6E3HAE_COND_IS_SUPPORT_LFD] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COND_IS_SUPPORT_LFD, s6e3hae_cond_is_support_lfd),
	[S6E3HAE_COND_IS_SUPPORT_LPM_LFD] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COND_IS_SUPPORT_LPM_LFD, s6e3hae_cond_is_support_lpm_lfd),
	[S6E3HAE_COND_IS_DISPLAY_ON] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COND_IS_DISPLAY_ON, s6e3hae_cond_is_display_on),
	[S6E3HAE_COND_IS_96HS_BASED_FPS] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COND_IS_96HS_BASED_FPS, s6e3hae_cond_is_96hs_based_fps),
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
	[S6E3HAE_COND_IS_BRIGHTDOT_ENABLED] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COND_IS_BRIGHTDOT_ENABLED, s6e3hae_cond_is_brightdot_enabled),
#endif
	[S6E3HAE_GETIDX_LPM_FPS_TABLE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_GETIDX_LPM_FPS_TABLE, s6e3hae_getidx_lpm_fps_table),
	[S6E3HAE_GETIDX_LFD_FRAME_INSERTION_TABLE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_GETIDX_LFD_FRAME_INSERTION_TABLE, s6e3hae_getidx_lfd_frame_insertion_table),
	[S6E3HAE_GETIDX_FFC_TABLE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_GETIDX_FFC_TABLE, s6e3hae_getidx_ffc_table),
	[S6E3HAE_GETIDX_VRR_MODE_TABLE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_GETIDX_VRR_MODE_TABLE, s6e3hae_getidx_vrr_mode_table),
	[S6E3HAE_COPY_LFD_MIN] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COPY_LFD_MIN, s6e3hae_maptbl_copy_lfd_min),
	[S6E3HAE_COPY_LFD_MAX] = __PNOBJ_FUNC_INITIALIZER(S6E3HAE_COPY_LFD_MAX, s6e3hae_maptbl_copy_lfd_max),
};

int s6e3hae_init(struct common_panel_info *cpi)
{
	static bool once;
	int ret;

	if (once)
		return 0;

	ret = panel_function_insert_array(s6e3hae_function_table,
			ARRAY_SIZE(s6e3hae_function_table));
	if (ret < 0)
		panel_err("failed to insert s6e3hae_function_table\n");

	cpi->prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array_size;
	cpi->prop_lists[USDM_DRV_LEVEL_DDI] = s6e3hae_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_DDI] = ARRAY_SIZE(s6e3hae_property_array);

	once = true;

	return 0;
}

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
