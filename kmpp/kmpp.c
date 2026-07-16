/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "kmpp"

#include <string.h>

#include "mpp_log.h"
#include "mpp_env.h"
#include "rk_mpi.h"
#include "mpp_impl.h"
#include "kmpp.h"
#include "kmpp_obj.h"
#include "kmpp_frame.h"
#include "kmpp_packet.h"

/* external ops table from kmpp_legacy.c */
extern KmppOps kmpp_legacy_ops;

void mpp_get_api(Kmpp *ctx)
{
    RK_U32 mode = 0;

    if (!ctx)
        return;

    mpp_env_get_u32("kmpp_mode", &mode, 0);

    /* validate mode, fall back to legacy on invalid value */
    if (mode != KMPP_MODE_OBJ && mode != KMPP_MODE_LEGACY)
        mode = KMPP_MODE_LEGACY;

    ctx->mMode = mode;
    ctx->mApi = &kmpp_legacy_ops;
}

void kmpp_release_venc_packet(void *ctx, void *arg)
{
    KmppPacket pkt = (KmppPacket)arg;

    if (!ctx || !pkt) {
        mpp_err_f("invalid input ctx %p pkt %p\n", ctx, pkt);
        return;
    }

    kmpp_packet_put(pkt);
}

