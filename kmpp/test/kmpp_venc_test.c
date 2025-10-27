/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_venc_test"

#include "rk_venc_kcfg.h"

#include "mpp_debug.h"
#include "mpp_common.h"
#include "kmpp_frame.h"
#include "kmpp_packet.h"
#include "kmpp_meta.h"
#include "kmpp_buffer.h"
#include "kmpp_venc.h"

#include "utils.h"
#include "mpi_enc_utils.h"

typedef struct KmppVencTestCtx_t {
    MpiEncTestArgs *cmd;
    KmppVenc venc;
    FILE *fp_in;
    FILE *fp_out;
    RK_U32 frame_size;
    RK_U32 width;
    RK_U32 height;
    RK_U32 hor_stride;
    RK_U32 ver_stride;
    RK_S32 frame_cnt;
    KmppBufGrp buf_grp;
} KmppVencTestCtx;

static MPP_RET venc_cfg_setup(KmppVencTestCtx *ctx)
{
    MppVencKcfg cfg;
    RK_S32 ret = rk_ok;
    MpiEncTestArgs *cmd = ctx->cmd;
    KmppVenc venc = ctx->venc;

    if (cmd->fps_in_den == 0)
        cmd->fps_in_den = 1;
    if (cmd->fps_in_num == 0)
        cmd->fps_in_num = 30;
    if (cmd->fps_out_den == 0)
        cmd->fps_out_den = 1;
    if (cmd->fps_out_num == 0)
        cmd->fps_out_num = 30;

    if (!cmd->bps_target)
        cmd->bps_target = cmd->width * cmd->height / 8 * (cmd->fps_out_num / cmd->fps_out_den);

    mpp_venc_kcfg_init(&cfg, MPP_VENC_KCFG_TYPE_ST_CFG);
    ret = kmpp_venc_get_cfg(venc, cfg);
    if (ret) {
        mpp_loge(MODULE_TAG " get venc cfg failed\n");
        return ret;
    }
    mpp_venc_kcfg_set_s32(cfg, "codec:type", cmd->type);
    mpp_venc_kcfg_set_s32(cfg, "prep:width", cmd->width);
    mpp_venc_kcfg_set_s32(cfg, "prep:height", cmd->height);
    mpp_venc_kcfg_set_s32(cfg, "prep:hor_stride", cmd->hor_stride);
    mpp_venc_kcfg_set_s32(cfg, "prep:ver_stride", cmd->ver_stride);
    mpp_venc_kcfg_set_s32(cfg, "prep:format", cmd->format);
    mpp_venc_kcfg_set_s32(cfg, "prep:range", 0);

    mpp_venc_kcfg_set_s32(cfg, "rc:fps_in_flex", 0);
    mpp_venc_kcfg_set_s32(cfg, "rc:fps_in_num", cmd->fps_in_num ? cmd->fps_in_num : 30);
    mpp_venc_kcfg_set_s32(cfg, "rc:fps_in_denom", 1);
    mpp_venc_kcfg_set_s32(cfg, "rc:fps_out_flex", 0);
    mpp_venc_kcfg_set_s32(cfg, "rc:fps_out_num", cmd->fps_out_num ? cmd->fps_out_num : 30);
    mpp_venc_kcfg_set_s32(cfg, "rc:fps_out_denom", 1);

    mpp_venc_kcfg_set_s32(cfg, "rc:mode", cmd->rc_mode);
    mpp_venc_kcfg_set_s32(cfg, "rc:bps_target", cmd->bps_target ? cmd->bps_target : 2000000);
    mpp_venc_kcfg_set_s32(cfg, "rc:bps_max", cmd->bps_max ? cmd->bps_max : 4000000);
    mpp_venc_kcfg_set_s32(cfg, "rc:bps_min", cmd->bps_min ? cmd->bps_min : 1000000);
    mpp_venc_kcfg_set_s32(cfg, "rc:gop", cmd->gop_len ? cmd->gop_len : 60);

    switch (cmd->type) {
    case MPP_VIDEO_CodingAVC : {
        mpp_venc_kcfg_set_s32(cfg, "h264:profile", 100);
        mpp_venc_kcfg_set_s32(cfg, "h264:level", 40);
        mpp_venc_kcfg_set_s32(cfg, "h264:cabac_en", 1);
        mpp_venc_kcfg_set_s32(cfg, "h264:cabac_idc", 0);
        mpp_venc_kcfg_set_s32(cfg, "h264:trans8x8", 1);
    } break;
    case MPP_VIDEO_CodingMJPEG : {
        mpp_venc_kcfg_set_s32(cfg, "jpeg:q_factor", cmd->qp_init ? cmd->qp_init : 80);
        mpp_venc_kcfg_set_s32(cfg, "jpeg:qf_max", cmd->qp_max ? cmd->qp_max : 99);
        mpp_venc_kcfg_set_s32(cfg, "jpeg:qf_min", cmd->qp_min ? cmd->qp_min : 1);
    } break;
    default: {
    } break;
    }

    switch (cmd->type) {
    case MPP_VIDEO_CodingHEVC :
    case MPP_VIDEO_CodingAVC : {
        switch (cmd->rc_mode) {
        case MPP_ENC_RC_MODE_FIXQP : {
            RK_S32 fix_qp = cmd->qp_init;

            mpp_venc_kcfg_set_s32(cfg, "rc:qp_init", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_max", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_min", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_max_i", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_min_i", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_ip", 0);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_min_i", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_max_i", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_min_p", fix_qp);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_max_p", fix_qp);
        } break;
        case MPP_ENC_RC_MODE_CBR :
        case MPP_ENC_RC_MODE_VBR :
        case MPP_ENC_RC_MODE_AVBR :
        case MPP_ENC_RC_MODE_SMTRC : {
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_init", cmd->qp_init ? cmd->qp_init : -1);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_max", cmd->qp_max ? cmd->qp_max : 51);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_min", cmd->qp_min ? cmd->qp_min : 10);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_max_i", cmd->qp_max_i ? cmd->qp_max_i : 51);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_min_i", cmd->qp_min_i ? cmd->qp_min_i : 10);
            mpp_venc_kcfg_set_s32(cfg, "rc:qp_ip", 2);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_min_i", cmd->fqp_min_i ? cmd->fqp_min_i : 10);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_max_i", cmd->fqp_max_i ? cmd->fqp_max_i : 45);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_min_p", cmd->fqp_min_p ? cmd->fqp_min_p : 10);
            mpp_venc_kcfg_set_s32(cfg, "rc:fqp_max_p", cmd->fqp_max_p ? cmd->fqp_max_p : 45);
        } break;
        default : {
            mpp_err_f("unsupport encoder rc mode %d\n", cmd->rc_mode);
        } break;
        }
    } break;
    case MPP_VIDEO_CodingMJPEG : {
        mpp_venc_kcfg_set_s32(cfg, "jpeg:q_factor", cmd->qp_init ? cmd->qp_init : 80);
        mpp_venc_kcfg_set_s32(cfg, "jpeg:qf_max", cmd->qp_max ? cmd->qp_max : 99);
        mpp_venc_kcfg_set_s32(cfg, "jpeg:qf_min", cmd->qp_min ? cmd->qp_min : 1);
    } break;
    default: {
    } break;
    }

    kmpp_venc_set_cfg(ctx->venc, cfg);

    mpp_venc_kcfg_deinit(cfg);

    return ret;
}

static MPP_RET venc_encode_oneframe(KmppVencTestCtx *ctx)
{
    KmppVenc venc = ctx->venc;
    KmppPacket packet = NULL;
    KmppFrame frame = NULL;
    KmppBuffer kbuf = NULL;
    KmppBufCfg buf_cfg = NULL;
    KmppShmPtr sptr, grp_sptr;
    rk_s32 frame_cnt = ctx->frame_cnt;

    /* allocate KmppBuffer and fill pixel data */
    kmpp_buffer_get(&kbuf);
    buf_cfg = kmpp_buffer_to_cfg(kbuf);

    grp_sptr = *kmpp_obj_to_shm(ctx->buf_grp);
    kmpp_buf_cfg_set_group(buf_cfg, &grp_sptr);
    kmpp_buf_cfg_set_size(buf_cfg, ctx->frame_size);

    kmpp_buffer_setup(kbuf);

    /* get data pointer for filling pixel data */
    kmpp_buf_cfg_get_sptr(buf_cfg, &sptr);
    fill_image(sptr.uptr, ctx->width, ctx->height,
               ctx->hor_stride, ctx->ver_stride, 0, frame_cnt);

    /* attach KmppBuffer object to frame via shm ptr */
    sptr = *kmpp_obj_to_shm(kbuf);
    kmpp_frame_get(&frame);
    kmpp_frame_set_buffer(frame, &sptr);
    kmpp_frame_set_width(frame, ctx->width);
    kmpp_frame_set_height(frame, ctx->height);
    kmpp_frame_set_hor_stride(frame, ctx->hor_stride);
    kmpp_frame_set_ver_stride(frame, ctx->ver_stride);
    kmpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
    kmpp_frame_set_eos(frame, 0);

    kmpp_venc_put_frm(venc, frame);

    kmpp_venc_get_pkt(venc, &packet);

    if (packet) {
        KmppFrame frame_out = NULL;
        KmppMeta meta = NULL;
        KmppShmPtr meta_sptr;
        rk_s32 len;

        kmpp_packet_get_length(packet, &len);
        mpp_logi("frame %d get packet length %d\n", frame_cnt, len);
        if (ctx->fp_out) {
            KmppShmPtr pos;

            kmpp_packet_get_pos(packet, &pos);
            fwrite(pos.uptr, 1, len, ctx->fp_out);
            fflush(ctx->fp_out);
        }

        kmpp_packet_get_meta(packet, &meta_sptr);
        kmpp_obj_get_by_sptr_f(&meta, &meta_sptr);
        if (meta) {
            kmpp_meta_get_obj(meta, KEY_INPUT_FRAME, (KmppObj *)&frame_out);

            if (frame_out) {
                KmppShmPtr *frm_sptr = kmpp_obj_to_shm(frame);
                KmppShmPtr *inf_sptr = kmpp_obj_to_shm(frame_out);

                if (frm_sptr && inf_sptr && frm_sptr->kptr != inf_sptr->kptr)
                    mpp_loge("frame %d shm mismatch: frame kptr %p in_frame kptr %p\n",
                             frame_cnt, frm_sptr->kptr, inf_sptr->kptr);
            }
        }

        kmpp_packet_put(packet);
    } else {
        mpp_loge("frame %d get no packet\n", frame_cnt);
    }

    kmpp_frame_put(frame);
    kmpp_buffer_put(kbuf);

    return rk_ok;
}

static MPP_RET venc_test_ctx_init(KmppVencTestCtx *ctx)
{
    MpiEncTestArgs *cmd = ctx->cmd;

    ctx->width = cmd->width;
    ctx->height = cmd->height;
    ctx->hor_stride = cmd->hor_stride ? cmd->hor_stride : MPP_ALIGN(cmd->width, 16);
    ctx->ver_stride = cmd->ver_stride ? cmd->ver_stride : MPP_ALIGN(cmd->height, 16);

    switch (cmd->format & MPP_FRAME_FMT_MASK) {
    case MPP_FMT_YUV420SP:
    case MPP_FMT_YUV420P: {
        ctx->frame_size = MPP_ALIGN(ctx->hor_stride, 64) * MPP_ALIGN(ctx->ver_stride, 64) * 3 / 2;
    } break;

    case MPP_FMT_YUV422_YUYV :
    case MPP_FMT_YUV422_YVYU :
    case MPP_FMT_YUV422_UYVY :
    case MPP_FMT_YUV422_VYUY :
    case MPP_FMT_YUV422P :
    case MPP_FMT_YUV422SP : {
        ctx->frame_size = MPP_ALIGN(ctx->hor_stride, 64) * MPP_ALIGN(ctx->ver_stride, 64) * 2;
    } break;
    case MPP_FMT_YUV400:
    case MPP_FMT_RGB444 :
    case MPP_FMT_BGR444 :
    case MPP_FMT_RGB555 :
    case MPP_FMT_BGR555 :
    case MPP_FMT_RGB565 :
    case MPP_FMT_BGR565 :
    case MPP_FMT_RGB888 :
    case MPP_FMT_BGR888 :
    case MPP_FMT_RGB101010 :
    case MPP_FMT_BGR101010 :
    case MPP_FMT_ARGB8888 :
    case MPP_FMT_ABGR8888 :
    case MPP_FMT_BGRA8888 :
    case MPP_FMT_RGBA8888 : {
        ctx->frame_size = MPP_ALIGN(ctx->hor_stride, 64) * MPP_ALIGN(ctx->ver_stride, 64);
    } break;

    default: {
        ctx->frame_size = MPP_ALIGN(ctx->hor_stride, 64) * MPP_ALIGN(ctx->ver_stride, 64) * 4;
    } break;
    }

    if (cmd->file_output) {
        FILE *fp = fopen(cmd->file_output, "w+b");

        if (!fp) {
            mpp_err_f("open file %s failed\n", cmd->file_output);
            return MPP_NOK;
        }
        ctx->fp_out = fp;
    }

    return MPP_OK;
}

int main(int argc, char **argv)
{
    MppEncTestObjSet *obj_set = NULL;
    MpiEncTestArgs *cmd = NULL;
    KmppVencTestCtx ctx;
    KmppVenc venc = NULL;
    MppVencKcfg cfg = NULL;
    rk_s32 ret = rk_ok;

    memset(&ctx, 0, sizeof(ctx));

    ret = mpi_enc_test_objset_get(&obj_set);
    if (ret)
        goto DONE;

    // parse the cmd option
    ret = mpi_enc_test_objset_update_by_args(obj_set, argc, argv, MODULE_TAG);
    if (ret)
        goto DONE;

    cmd = obj_set->cmd;
    ctx.cmd = cmd;
    ret = venc_test_ctx_init(&ctx);
    if (ret)
        goto DONE;

    mpp_logi(MODULE_TAG " start\n");

    ret = kmpp_venc_get(&venc);
    if (ret) {
        mpp_loge(MODULE_TAG " get venc failed\n");
        goto DONE;
    }
    ctx.venc = venc;

    mpp_venc_kcfg_init(&cfg, MPP_VENC_KCFG_TYPE_INIT);
    mpp_venc_kcfg_set_u32(cfg, "type", MPP_CTX_ENC);
    mpp_venc_kcfg_set_u32(cfg, "coding", cmd->type);
    mpp_venc_kcfg_set_s32(cfg, "chan_id", 0);
    mpp_venc_kcfg_set_u32(cfg, "max_width", cmd->width);
    mpp_venc_kcfg_set_u32(cfg, "max_height", cmd->height);
    mpp_venc_kcfg_set_u32(cfg, "max_lt_cnt", 0);
    mpp_venc_kcfg_set_s32(cfg, "input_timeout", -1);
    mpp_venc_kcfg_set_s32(cfg, "ntfy_mode", 0);

    ret = kmpp_venc_init(venc, cfg);
    if (ret) {
        mpp_loge(MODULE_TAG " init venc failed\n");
        mpp_venc_kcfg_deinit(cfg);
        goto DONE;
    }

    mpp_venc_kcfg_deinit(cfg);
    cfg = NULL;

    ret = venc_cfg_setup(&ctx);
    if (ret) {
        mpp_loge(MODULE_TAG " setup venc cfg failed\n");
        goto DONE;
    }

    ret = kmpp_venc_start(venc);
    if (ret) {
        mpp_loge(MODULE_TAG " start venc failed\n");
        goto DONE;
    }

    /* init KmppBufGrp for KmppBuffer allocation */
    kmpp_buf_grp_get(&ctx.buf_grp);
    {
        KmppBufGrpCfg grp_cfg = kmpp_buf_grp_to_cfg(ctx.buf_grp);

        kmpp_buf_grp_cfg_set_count(grp_cfg, 16);
        kmpp_buf_grp_cfg_set_size(grp_cfg, ctx.frame_size);
        kmpp_buf_grp_setup(ctx.buf_grp);
    }

    while (ctx.frame_cnt < cmd->frame_num) {
        ret = venc_encode_oneframe(&ctx);
        if (ret) {
            mpp_loge(MODULE_TAG " encode one frame failed\n");
            break;
        }
        ctx.frame_cnt++;
    }

DONE:
    if (ctx.buf_grp) {
        kmpp_buf_grp_put(ctx.buf_grp);
        ctx.buf_grp = NULL;
    }

    if (ctx.fp_out) {
        fclose(ctx.fp_out);
        ctx.fp_out = NULL;
    }

    if (ctx.venc) {
        ret = kmpp_venc_stop(venc);
        if (ret)
            mpp_loge(MODULE_TAG " stop venc failed\n");

        ret = kmpp_venc_deinit(venc);
        if (ret)
            mpp_loge(MODULE_TAG " deinit venc failed\n");

        kmpp_venc_put(ctx.venc);
    }

    mpi_enc_test_objset_put(obj_set);

    mpp_logi(MODULE_TAG " %s\n", ret ? "failed" : "success");
    return ret;
}
