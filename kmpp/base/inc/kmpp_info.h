/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 */

#ifndef KMPP_INFO_H
#define KMPP_INFO_H

#include "rk_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KMPP_INFO_VENC_CTRL_CFG = 0,   /* venc/feat :: ctrl_cfg */
    KMPP_INFO_BUTT,
} KmppInfoId;

rk_s32 kmpp_info_flag(KmppInfoId id);

#ifdef __cplusplus
}
#endif

#endif /* KMPP_INFO_H */
