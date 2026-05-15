/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_packet_impl.h"
#include "kmpp_meta.h"

static rk_s32 kmpp_packet_impl_deinit(void *entry, KmppObj obj, const char *caller)
{
    KmppShmPtr meta_sptr;

    (void)entry;
    (void)caller;

    if (kmpp_packet_get_meta(obj, &meta_sptr) == rk_ok) {
        KmppMeta meta = NULL;

        kmpp_obj_get_by_sptr_f(&meta, &meta_sptr);
        if (meta)
            kmpp_meta_put_f(meta);
    }

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_packet
#define KMPP_OBJ_INTF_TYPE          KmppPacket
#define KMPP_OBJ_IMPL_TYPE          KmppPacketImpl
#define KMPP_OBJ_FUNC_DEINIT        kmpp_packet_impl_deinit
#define KMPP_OBJ_SGLN_ID            MPP_SGLN_KMPP_PACKET
#define KMPP_OBJ_ENTRY_TABLE        KMPP_PACKET_ENTRY_TABLE
#include "kmpp_obj_helper.h"
