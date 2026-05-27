/*
 * Copyright 2022 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VEPU5XX_COMMON_H
#define VEPU5XX_COMMON_H

#include "rk_venc_cmd.h"
#include "mpp_device.h"
#include "hal_dbg.h"

#define VEPU_REG_RD(dev, regs, field, off, ret_acc) do { \
    _rd_cfg.reg = &((regs)->field);                       \
    _rd_cfg.size = sizeof((regs)->field);                 \
    _rd_cfg.offset = off;                                 \
    (ret_acc) |= mpp_dev_ioctl(dev, MPP_DEV_REG_RD, &_rd_cfg); \
} while (0)

#define VEPU_REG_RD_PTR(dev, ptr, off, ret_acc) do { \
    _rd_cfg.reg = (ptr);                              \
    _rd_cfg.size = sizeof(*(ptr));                    \
    _rd_cfg.offset = off;                             \
    (ret_acc) |= mpp_dev_ioctl(dev, MPP_DEV_REG_RD, &_rd_cfg); \
} while (0)

#define vepu_sw_regs(dbg_ctx, regs, offset, mode) \
    hal_dbg_dump_set_regs(dbg_ctx, (RK_U32 *)&(regs), sizeof(regs) / sizeof(RK_U32), \
                          offset / sizeof(RK_U32), mode)

#define vepu_hw_regs(dbg_ctx, regs, offset, mode) \
    hal_dbg_dump_get_regs(dbg_ctx, (RK_U32 *)&(regs), sizeof(regs) / sizeof(RK_U32), \
                          offset / sizeof(RK_U32), mode)

#define vepu_dump_fbc_buf(dbg_ctx, prefix, hal_buf, fbc_hdr, line_bits) do { \
    size_t _total = mpp_buffer_get_size((hal_buf)->buf[0]); \
    size_t _dsp   = mpp_buffer_get_size((hal_buf)->buf[1]); \
    hal_dbg_dumpf_buf(dbg_ctx, prefix "fbh.bin", (hal_buf)->buf[0], 0,       fbc_hdr,          line_bits, "w+"); \
    hal_dbg_dumpf_buf(dbg_ctx, prefix "fbd.bin", (hal_buf)->buf[0], fbc_hdr,  _total - fbc_hdr, line_bits, "w+"); \
    hal_dbg_dumpf_buf(dbg_ctx, prefix "dsp.bin", (hal_buf)->buf[1], 0,        _dsp,             line_bits, "w+"); \
} while (0)

/*
 * Invert color threshold is for the absolute difference between background
 * and foregroud color.
 * If background color and foregroud color are close enough then trigger the
 * invert color process.
 */
#define ENC_DEFAULT_OSD_INV_THR         15
#define SET_OSD_INV_THR(index, reg, region)\
    if(region[index].inverse)   \
        reg.osd_ithd_r##index = ENC_DEFAULT_OSD_INV_THR;

typedef enum Vepu5xxOsdPltType_e {
    VEPU5xx_OSD_PLT_TYPE_USERDEF    = 0,
    VEPU5xx_OSD_PLT_TYPE_DEFAULT    = 1,
} Vepu5xxOsdPltType;

typedef enum VepuFmt_e {
    VEPU5xx_FMT_BGRA8888,   // 0
    VEPU5xx_FMT_BGR888,     // 1
    VEPU5xx_FMT_BGR565,     // 2
    VEPU5xx_FMT_ARGB1555,   // 3
    VEPU5xx_FMT_YUV422SP,   // 4
    VEPU5xx_FMT_YUV422P,    // 5
    VEPU5xx_FMT_YUV420SP,   // 6
    VEPU5xx_FMT_YUV420P,    // 7
    VEPU5xx_FMT_YUYV422,    // 8
    VEPU5xx_FMT_UYVY422,    // 9
    VEPU5xx_FMT_YUV400,     // 10
    VEPU5xx_FMT_AYUV2BPP,   // 11
    VEPU5xx_FMT_YUV444SP,   // 12
    VEPU5xx_FMT_YUV444P,    // 13
    VEPU5xx_FMT_ARGB4444,   // 14
    VEPU5xx_FMT_AYUV1BPP,   // 15
    VEPU5xx_FMT_BUTT,       // 16
} VepuFmt;

typedef struct VepuFmtCfg_t {
    VepuFmt         format;
    RK_U32          alpha_swap;
    RK_U32          rbuv_swap;
    RK_U32          src_range;
    RK_U32          src_endian;
    const RK_S32    *weight;
    const RK_S32    *offset;
} VepuFmtCfg;

typedef struct Vepu5xxOsdCfg_t {
    void                *reg_base;
    MppDev              dev;
    MppDevRegOffCfgs    *reg_cfg;
    MppEncOSDPltCfg     *plt_cfg;
    MppEncOSDData       *osd_data;
    MppEncOSDData2      *osd_data2;
} Vepu5xxOsdCfg;

typedef struct VepuRgb2YuvCoeffs_t {
    RK_S16 r_coeff;
    RK_S16 g_coeff;
    RK_S16 b_coeff;
    RK_S16 offset;
} VepuRgb2YuvCoeffs;

/* formula: y(r, g, b) = (r_coeff x r + g_coeff x g + b_coeff x b + 128) >> 8 + offset */
typedef struct VepuRgb2YuvCfg_t {
    /* YUV colorspace type */
    MppFrameColorSpace color;
    /* MPEG vs JPEG YUV range */
    MppFrameColorRange dst_range;
    /* coeffs of rgb 2 yuv */
    VepuRgb2YuvCoeffs  _2y;
    VepuRgb2YuvCoeffs  _2u;
    VepuRgb2YuvCoeffs  _2v;
} VepuRgb2YuvCfg;

/**
 * @brief Get rgb2yuv cfg by color range and color space. If not found, return default cfg.
 *        default cfg's yuv range - limit.
 *        default cfg's color space - BT.601.
 */
const VepuRgb2YuvCfg* get_rgb2yuv_cfg(MppFrameColorRange range, MppFrameColorSpace color);

MPP_RET copy2osd2(MppEncOSDData2* dst, MppEncOSDData *src1, MppEncOSDData2 *src2);

MPP_RET vepu5xx_set_fmt(VepuFmtCfg *cfg, MppFrameFormat format);

extern const RK_U32 vepu580_540_h264_flat_scl_tab[576];

extern const RK_U32 klut_weight[24];
extern const RK_U32 lamd_satd_qp[52];
extern const RK_U32 lamd_moda_qp[52];
extern const RK_U32 lamd_modb_qp[52];
extern const RK_U32 lamd_satd_qp_510[52];

#endif /* VEPU5XX_COMMON_H */