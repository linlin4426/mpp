/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp"

#include "rk_mpi.h"

#include "mpp_log.h"
#include "mpp_env.h"
#include "mpp_thread.h"

#include "kmpp.h"
#include "kmpp_packet.h"

/* kmpp path mode */
typedef enum KmppMode_e {
    KMPP_MODE_AUTO      = 0,  /* auto-detect / no kmpp wrapper */
    KMPP_MODE_LEGACY    = 1,  /* legacy /dev/vcodec path */
    KMPP_MODE_OBJ       = 2,  /* kmpp_obj path via /dev/kmpp_objs + /dev/kmpp_ioctl */
    KMPP_MODE_BUTT,
} KmppMode;

extern KmppOps kmpp_legacy_ops;
extern KmppOps kmpp_venc_obj_ops;

static RK_U32 kmpp_mode = 0;
static pthread_once_t kmpp_mode_once = PTHREAD_ONCE_INIT;

static void kmpp_mode_init(void)
{
    mpp_env_get_u32("kmpp_mode", &kmpp_mode, 0);
}

void mpp_get_api(Kmpp *ctx)
{
    if (!ctx)
        return;

    pthread_once(&kmpp_mode_once, kmpp_mode_init);

    if (kmpp_mode == KMPP_MODE_AUTO) {
        /* default to legacy mode */
        kmpp_mode = KMPP_MODE_LEGACY;
    }

    switch (kmpp_mode) {
    case KMPP_MODE_OBJ : {
        ctx->mApi = &kmpp_venc_obj_ops;
    } break;
    case KMPP_MODE_LEGACY : {
        ctx->mApi = &kmpp_legacy_ops;
    } break;
    default : {
        mpp_loge_f("invalid kmpp mode %d, fallback to legacy mode\n");
        kmpp_mode = KMPP_MODE_LEGACY;
        ctx->mApi = &kmpp_legacy_ops;
    } break;
    }

    ctx->mMode = kmpp_mode;
}

void kmpp_release_venc_packet(void *ctx, void *arg)
{
    KmppPacket pkt = (KmppPacket)arg;

    if (!ctx || !pkt) {
        mpp_loge_f("invalid input ctx %p pkt %p\n", ctx, pkt);
        return;
    }

    kmpp_packet_put(pkt);
}
