/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_venc_utils"

#include <string.h>
#include <limits.h>

#include "mpp_log.h"
#include "mpp_env.h"
#include "rk_venc_cmd.h"

#include "kmpp_meta.h"
#include "mpp_common.h"
#include "kmpp_venc_utils.h"

static RK_U32 venc_utils_debug = 0;

#define venc_utils_dbg(fmt, ...) \
    mpp_logi_c(venc_utils_debug, fmt, ## __VA_ARGS__)

#define MPP_ENC_FRM_JPEG_ROI_LEVEL_MAX  63

/* Standard test UUID for unregistered user data SEI */
RK_U8 venc_test_uuid[16] = {
    0xfe, 0x39, 0xac, 0x4c, 0x4a, 0x8e, 0x4b, 0x4b,
    0x85, 0xd9, 0xb2, 0xa2, 0x4f, 0xa1, 0x19, 0x5b,
};

static int find_nal_start(const RK_U8 *data, RK_S32 len, RK_S32 *offset)
{
    RK_S32 i = *offset;
    RK_S32 zeros = 0;

    for (; i < len - 2; i++) {
        if (data[i] == 0) {
            zeros++;
        } else if (data[i] == 1 && zeros >= 2) {
            *offset = i + 1;
            return (zeros >= 3) ? 4 : 3;
        } else {
            zeros = 0;
        }
    }

    return 0;
}

RK_S32 kmpp_venc_scan_sei_userdata(const RK_U8 *data, RK_S32 len, const char *expect, RK_S32 expect_len)
{
    RK_S32 offset = 0;

    while (offset < len - 4) {
        if (!find_nal_start(data, len, &offset) || offset >= len)
            break;

        /* check SEI NAL type */
        if ((data[offset] & 0x1F) != 6)
            continue;

        /* parse SEI payload_type and payload_size */
        {
            RK_S32 pos = offset + 1;
            RK_S32 payload_type = 0;
            RK_S32 payload_size = 0;

            while (pos < len && data[pos] == 0xff) {
                payload_type += 255;
                pos++;
            }
            if (pos < len) {
                payload_type += data[pos];
                pos++;
            }

            if (payload_type != 5)
                continue;

            while (pos < len && data[pos] == 0xff) {
                payload_size += 255;
                pos++;
            }
            if (pos < len) {
                payload_size += data[pos];
                pos++;
            }

            /* check UUID match */
            if (pos + 16 > len)
                break;

            if (memcmp(data + pos, venc_test_uuid, 16) != 0)
                continue;

            pos += 16;

            /* verify payload content */
            {
                RK_S32 ud_len = payload_size - 16;

                if (ud_len <= 0 || pos + ud_len > len)
                    break;

                if (ud_len >= expect_len && memcmp(data + pos, expect, expect_len) == 0)
                    return 1;
            }
        }
    }

    return 0;
}

MPP_RET kmpp_venc_gen_userdata(KmppMeta meta, RK_U8 *ud_buf, RK_U32 ud_buf_size)
{
    MppEncUserDataShm ud;

    if (!meta || !ud_buf || !ud_buf_size)
        return MPP_ERR_NULL_PTR;

    memset(&ud, 0, sizeof(ud));
    ud.len = ud_buf_size;
    ud.data.uptr = ud_buf;

    if (kmpp_meta_set_ptr(meta, KEY_USER_DATA, &ud))
        mpp_loge("KEY_USER_DATA not set, skipped\n");
    venc_utils_dbg("set KEY_USER_DATA len %d\n", ud_buf_size);

    return MPP_OK;
}

MPP_RET kmpp_venc_gen_userdatas(KmppMeta meta, const RK_U8 *uuid,
                                RK_U8 *ud_buf, RK_U32 ud_buf_size)
{
    MPP_ENC_UDS1(uds_s);
    MppEncUserDataFullShm *entry = uds_s.set.data;
    RK_U8 uuid_buf[MPP_ENC_USER_DATA_UUID_LEN];

    if (!meta || !ud_buf || !ud_buf_size)
        return MPP_ERR_NULL_PTR;

    memset(&uds_s, 0, sizeof(uds_s));
    uds_s.set.count = 1;
    entry->len = ud_buf_size;
    if (uuid) {
        memcpy(uuid_buf, uuid, sizeof(uuid_buf));
        entry->uuid.uptr = uuid_buf;
    }
    entry->data.uptr = ud_buf;

    if (kmpp_meta_set_ptr(meta, KEY_USER_DATAS, &uds_s.set))
        mpp_loge("KEY_USER_DATAS not set, skipped\n");
    venc_utils_dbg("set KEY_USER_DATAS len %d\n", ud_buf_size);

    return MPP_OK;
}

/*
 * Convert Q16 fixed-point ratio to pixel coordinate.
 *
 * px = (ratio * size) >> 16, clamped to [0, size - 1].
 * Ratio >= COORD_MAX maps to size - 1 (edge of frame), negative maps to 0.
 */
static RK_U32 frm_cfg_px(RK_S32 ratio, RK_U32 size)
{
    RK_U32 px;

    if (size <= 0)
        return 0;

    if (ratio <= 0)
        return 0;

    if (ratio >= MPP_ENC_FRM_CFG_COORD_MAX)
        return size - 1;

    /* (ratio * size) <= (2^16 - 1) * size, 32-bit safe up to 64K picture */
    px = (RK_U32)(ratio * (RK_S32)size) >> 16;

    if (px >= size)
        px = size - 1;

    return px;
}

MPP_RET kmpp_venc_gen_roi(KmppMeta meta, RK_U32 w, RK_U32 h,
                          RK_U32 roi_cnt, const MppEncFrmRoi *roi)
{
    MppEncROICfgLegacy cfg = { .change = 1 };
    RK_U32 i;

    if (!meta || !roi || !roi_cnt || roi_cnt > 8)
        return MPP_ERR_NULL_PTR;

    cfg.number = roi_cnt;
    for (i = 0; i < roi_cnt; i++) {
        if (roi[i].x < 0 || roi[i].x > MPP_ENC_FRM_CFG_COORD_MAX ||
            roi[i].y < 0 || roi[i].y > MPP_ENC_FRM_CFG_COORD_MAX ||
            roi[i].w <= 0 || roi[i].w > MPP_ENC_FRM_CFG_COORD_MAX ||
            roi[i].h <= 0 || roi[i].h > MPP_ENC_FRM_CFG_COORD_MAX ||
            roi[i].intra < 0 || roi[i].intra > USHRT_MAX ||
            roi[i].quality < SHRT_MIN || roi[i].quality > SHRT_MAX ||
            roi[i].abs_qp_en < 0 || roi[i].abs_qp_en > UCHAR_MAX)
            return MPP_ERR_VALUE;
        RK_U32 x = frm_cfg_px(roi[i].x, w);
        RK_U32 y = frm_cfg_px(roi[i].y, h);
        RK_U32 rw = frm_cfg_px(roi[i].w, w);
        RK_U32 rh = frm_cfg_px(roi[i].h, h);

        /* clamp: region must stay inside frame, kernel rejects x+w > w */
        if (x + rw > w)
            rw = w - x;
        if (y + rh > h)
            rh = h - y;

        cfg.regions[i].x         = (RK_U16)x;
        cfg.regions[i].y         = (RK_U16)y;
        cfg.regions[i].w         = (RK_U16)rw;
        cfg.regions[i].h         = (RK_U16)rh;
        cfg.regions[i].intra     = (RK_U16)roi[i].intra;
        cfg.regions[i].quality   = (RK_S16)roi[i].quality;
        cfg.regions[i].abs_qp_en = (RK_U8)roi[i].abs_qp_en;
    }

    kmpp_meta_set_ptr(meta, KEY_ROI_DATA, &cfg);
    venc_utils_dbg("ROI cnt=%d r0=(%d,%d,%dx%d) i=%d q=%d\n",
                   roi_cnt, cfg.regions[0].x, cfg.regions[0].y,
                   cfg.regions[0].w, cfg.regions[0].h,
                   cfg.regions[0].intra, cfg.regions[0].quality);

    return MPP_OK;
}

/*
 * Auto-derive OSD stride from region width and format when stride == 0.
 * Pixel bytes per format matches vepu5xx_check_osd_buffer_size.
 */
static RK_U32 frm_cfg_osd_stride(RK_U32 width, RK_U32 fmt)
{
    RK_U32 ret = width;

    switch (fmt) {
    case MPP_FMT_ARGB8888 :
    case MPP_FMT_ABGR8888 :
    case MPP_FMT_BGRA8888 :
    case MPP_FMT_RGBA8888 : {
        ret = width * 4;
    } break;
    case MPP_FMT_ARGB1555 :
    case MPP_FMT_RGB565 :
    case MPP_FMT_BGR565 : {
        ret = width * 2;
    } break;
    default : {
    } break;
    }

    return ret;
}

MPP_RET kmpp_venc_gen_osd(KmppMeta meta, RK_U32 w, RK_U32 h,
                          RK_U32 osd_cnt, const MppEncFrmOsd *osd)
{
    MppEncOSDData3 data = { .change = 1 };
    RK_U32 i;

    if (!meta || !osd || !osd_cnt || osd_cnt > 8)
        return MPP_ERR_NULL_PTR;

    data.num_region = osd_cnt;
    for (i = 0; i < osd_cnt && i < 8; i++) {
        RK_U32 lt_x = frm_cfg_px(osd[i].lt_x, w);
        RK_U32 lt_y = frm_cfg_px(osd[i].lt_y, h);
        RK_U32 rb_x = frm_cfg_px(osd[i].rb_x, w);
        RK_U32 rb_y = frm_cfg_px(osd[i].rb_y, h);

        /* clamp: rb corner must be after lt corner, inside frame */
        if (rb_x < lt_x)
            rb_x = lt_x;
        if (rb_y < lt_y)
            rb_y = lt_y;

        data.region[i].enable         = osd[i].enable;
        data.region[i].range_trns_en  = osd[i].range_trns_en;
        data.region[i].range_trns_sel = osd[i].range_trns_sel;
        data.region[i].fmt            = osd[i].fmt;
        data.region[i].rbuv_swap      = osd[i].rbuv_swap;
        data.region[i].lt_x           = lt_x;
        data.region[i].lt_y           = lt_y;
        data.region[i].rb_x           = rb_x;
        data.region[i].rb_y           = rb_y;
        data.region[i].stride         = osd[i].stride ? osd[i].stride :
                                        frm_cfg_osd_stride(rb_x - lt_x + 1, osd[i].fmt);
        data.region[i].ch_ds_mode     = osd[i].ch_ds_mode;
        data.region[i].osd_endn       = osd[i].osd_endn;
    }

    kmpp_meta_set_ptr(meta, KEY_OSD_DATA4, &data);
    venc_utils_dbg("OSD cnt=%d r0=(%d,%d)-(%d,%d)\n",
                   osd_cnt, data.region[0].lt_x, data.region[0].lt_y,
                   data.region[0].rb_x, data.region[0].rb_y);

    return MPP_OK;
}

static void set_scalar_or_skip(KmppMeta meta, RK_S32 key, RK_S32 val)
{
    if (val >= 0 && kmpp_meta_set_s32(meta, key, val))
        venc_utils_dbg("frame scalar key %#08x not set, skipped\n", key);
}

static MPP_RET mpp_venc_check_frame_scalars(const MppEncFrmCfg *entry)
{
    if (entry->input_idr_req < -1 || entry->input_idr_req > 1 ||
        entry->input_pskip < -1 || entry->input_pskip > 1 ||
        entry->input_pskip_non_ref < -1 || entry->input_pskip_non_ref > 1 ||
        entry->input_pskip_num < -1 ||
        entry->enc_mark_ltr < -1 || entry->enc_use_ltr < -1 ||
        entry->enc_frame_qp < -1 || entry->enc_frame_qp > 51 ||
        entry->enc_base_layer_pid < -1 || entry->temporal_id < -1 ||
        (entry->input_pskip > 0 && entry->input_pskip_non_ref > 0))
        return MPP_ERR_VALUE;

    return MPP_OK;
}

static MPP_RET kmpp_venc_set_frame_scalars(KmppMeta meta, const MppEncFrmCfg *entry)
{
    if (entry->input_pskip > 0 && entry->input_pskip_non_ref > 0) {
        mpp_loge_f("pskip and pskip_non_ref cannot coexist\n");
        return MPP_ERR_VALUE;
    }

    set_scalar_or_skip(meta, KEY_INPUT_IDR_REQ, entry->input_idr_req);
    set_scalar_or_skip(meta, KEY_INPUT_PSKIP, entry->input_pskip);
    set_scalar_or_skip(meta, KEY_INPUT_PSKIP_NON_REF, entry->input_pskip_non_ref);
    set_scalar_or_skip(meta, KEY_INPUT_PSKIP_NUM, entry->input_pskip_num);
    set_scalar_or_skip(meta, KEY_ENC_MARK_LTR, entry->enc_mark_ltr);
    set_scalar_or_skip(meta, KEY_ENC_USE_LTR, entry->enc_use_ltr);
    set_scalar_or_skip(meta, KEY_ENC_FRAME_QP, entry->enc_frame_qp);
    set_scalar_or_skip(meta, KEY_ENC_BASE_LAYER_PID, entry->enc_base_layer_pid);
    set_scalar_or_skip(meta, KEY_TEMPORAL_ID, entry->temporal_id);

    return MPP_OK;
}

static MPP_RET kmpp_venc_gen_jpeg_roi(KmppMeta meta, RK_U32 w, RK_U32 h,
                                      RK_U32 jpeg_roi_cnt,
                                      const MppEncFrmJpegRoi *jpeg_roi,
                                      RK_U32 non_roi_level, RK_U32 non_roi_en)
{
    MppJpegROICfg cfg = { .change = 1 };
    RK_U32 i;

    if (!meta || !jpeg_roi || !jpeg_roi_cnt)
        return MPP_ERR_NULL_PTR;
    if (jpeg_roi_cnt > MPP_ARRAY_ELEMS(cfg.regions) ||
        non_roi_level > MPP_ENC_FRM_JPEG_ROI_LEVEL_MAX ||
        non_roi_en > 1)
        return MPP_ERR_VALUE;

    cfg.non_roi_level = non_roi_level;
    cfg.non_roi_en = non_roi_en;
    for (i = 0; i < jpeg_roi_cnt; i++) {
        const MppEncFrmJpegRoi *src = &jpeg_roi[i];

        if (src->x < 0 || src->x > MPP_ENC_FRM_CFG_COORD_MAX ||
            src->y < 0 || src->y > MPP_ENC_FRM_CFG_COORD_MAX ||
            src->w <= 0 || src->w > MPP_ENC_FRM_CFG_COORD_MAX ||
            src->h <= 0 || src->h > MPP_ENC_FRM_CFG_COORD_MAX ||
            src->level < 0 ||
            src->level > MPP_ENC_FRM_JPEG_ROI_LEVEL_MAX ||
            src->roi_en < 0 || src->roi_en > 1)
            return MPP_ERR_VALUE;

        cfg.regions[i].x = frm_cfg_px(src->x, w);
        cfg.regions[i].y = frm_cfg_px(src->y, h);
        cfg.regions[i].w = frm_cfg_px(src->w, w);
        cfg.regions[i].h = frm_cfg_px(src->h, h);
        cfg.regions[i].level = src->level;
        cfg.regions[i].roi_en = src->roi_en;
    }

    return kmpp_meta_set_ptr(meta, KEY_JPEG_ROI_DATA, &cfg);
}

MPP_RET kmpp_venc_gen_frame_meta(KmppMeta meta, RK_U32 w, RK_U32 h,
                                 const MppEncFrmCfg *entry)
{
    MPP_RET ret = MPP_OK;

    if (!meta || !entry)
        return MPP_ERR_NULL_PTR;

    mpp_env_get_u32("kmpp_venc_utils_debug", &venc_utils_debug, 0);

    ret = mpp_venc_check_frame_scalars(entry);
    if (ret)
        return ret;
    ret = kmpp_venc_set_frame_scalars(meta, entry);
    if (ret)
        return ret;

    if (entry->userdatas) {
        const RK_U8 *uds_uuid = entry->ud_uuid ? entry->ud_uuid : venc_test_uuid;

        ret = kmpp_venc_gen_userdatas(meta, uds_uuid, entry->ud_buf, entry->ud_buf_size);
        if (ret)
            return ret;
    }

    if (entry->userdata) {
        ret = kmpp_venc_gen_userdata(meta, entry->ud_buf, entry->ud_buf_size);
        if (ret)
            return ret;
    }

    if (entry->roi_cnt) {
        ret = kmpp_venc_gen_roi(meta, w, h, entry->roi_cnt, MPP_ENC_FRM_ROI_ARR(entry));
        if (ret)
            return ret;
    }

    if (entry->osd_cnt) {
        ret = kmpp_venc_gen_osd(meta, w, h, entry->osd_cnt, MPP_ENC_FRM_OSD_ARR(entry));
        if (ret)
            return ret;
    }

    if (entry->jpeg_roi_cnt) {
        ret = kmpp_venc_gen_jpeg_roi(meta, w, h, entry->jpeg_roi_cnt,
                                     MPP_ENC_FRM_JPEG_ROI_ARR(entry),
                                     entry->jpeg_non_roi_level,
                                     entry->jpeg_non_roi_en);
        if (ret)
            return ret;
    }

    return MPP_OK;
}
