/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "kmpp_info"

#include <pthread.h>

#include "mpp_runtime.h"

#include "kmpp_info.h"

typedef struct {
    const char *module;
    const char *kind;
    const char *name;
} KmppCapMap;

static const KmppCapMap kmpp_cap_map[KMPP_INFO_BUTT] = {
    { "venc", "feat", "ctrl_cfg" },         /* KMPP_INFO_VENC_CTRL_CFG    */
};

static rk_s32 kmpp_caps[KMPP_INFO_BUTT];
static pthread_once_t kmpp_info_once = PTHREAD_ONCE_INIT;

static void kmpp_info_init(void)
{
    const KmppCapMap *maps = kmpp_cap_map;
    rk_s32 i;

    for (i = 0; i < KMPP_INFO_BUTT; i++)
        kmpp_caps[i] = mpp_rt_kmpp_cap_check(maps[i].module, maps[i].kind, maps[i].name);
}

rk_s32 kmpp_info_flag(KmppInfoId id)
{
    rk_s32 idx = (rk_s32)id;

    if (idx < 0 || idx >= KMPP_INFO_BUTT)
        return 0;

    pthread_once(&kmpp_info_once, kmpp_info_init);

    return kmpp_caps[idx];
}
