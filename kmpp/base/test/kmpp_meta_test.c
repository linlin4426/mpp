/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_meta_test"

#include <stdlib.h>
#include <string.h>

#include "mpp_mem.h"
#include "mpp_time.h"
#include "mpp_debug.h"
#include "mpp_thread.h"
#include "kmpp_meta_impl.h"

#define THRD_DEFAULT    4
#define LOOP_DEFAULT    10

typedef struct {
    MppThread       *thread;
    RK_S32          loop_cnt;
    RK_S64          time_avg;
} MetaTestCtx;

static MPP_RET meta_set(KmppMeta meta)
{
    KmppShmPtr zero = {0};
    MPP_RET ret = MPP_OK;

    ret |= kmpp_meta_set_shm(meta, KEY_INPUT_FRAME, &zero);
    ret |= kmpp_meta_set_shm(meta, KEY_INPUT_PACKET, &zero);
    ret |= kmpp_meta_set_shm(meta, KEY_OUTPUT_FRAME, &zero);
    ret |= kmpp_meta_set_shm(meta, KEY_OUTPUT_PACKET, &zero);

    ret |= kmpp_meta_set_shm(meta, KEY_MOTION_INFO, &zero);
    ret |= kmpp_meta_set_shm(meta, KEY_HDR_INFO, &zero);

    ret |= kmpp_meta_set_s32(meta, KEY_INPUT_BLOCK, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_OUTPUT_BLOCK, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_INPUT_IDR_REQ, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_OUTPUT_INTRA, 0);

    ret |= kmpp_meta_set_s32(meta, KEY_TEMPORAL_ID, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_LONG_REF_IDX, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_ENC_AVERAGE_QP, 0);

    //ret |= kmpp_meta_set_shm(meta, KEY_ROI_DATA, NULL);
    ret |= kmpp_meta_set_shm(meta, KEY_OSD_DATA, NULL);
    ret |= kmpp_meta_set_shm(meta, KEY_OSD_DATA2, NULL);
    ret |= kmpp_meta_set_shm(meta, KEY_USER_DATA, NULL);
    ret |= kmpp_meta_set_shm(meta, KEY_USER_DATAS, NULL);

    ret |= kmpp_meta_set_shm(meta, KEY_QPMAP0, NULL);
    ret |= kmpp_meta_set_shm(meta, KEY_NPU_SOBJ_FLAG, NULL);
    ret |= kmpp_meta_set_ptr(meta, KEY_NPU_UOBJ_FLAG, NULL);

    ret |= kmpp_meta_set_s32(meta, KEY_ENC_MARK_LTR, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_ENC_USE_LTR, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_ENC_FRAME_QP, 0);
    ret |= kmpp_meta_set_s32(meta, KEY_ENC_BASE_LAYER_PID, 0);

    return ret;
}

static MPP_RET meta_get(KmppMeta meta)
{
    KmppShmPtr shm;
    void *ptr;
    RK_S32 val;
    MPP_RET ret = MPP_OK;

    ret |= kmpp_meta_get_shm(meta, KEY_INPUT_FRAME, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_INPUT_PACKET, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_OUTPUT_FRAME, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_OUTPUT_PACKET, &shm);

    ret |= kmpp_meta_get_shm(meta, KEY_MOTION_INFO, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_HDR_INFO, &shm);

    ret |= kmpp_meta_get_s32(meta, KEY_INPUT_BLOCK, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_OUTPUT_BLOCK, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_INPUT_IDR_REQ, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_OUTPUT_INTRA, &val);

    ret |= kmpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &val);

    //ret |= kmpp_meta_get_shm(meta, KEY_ROI_DATA, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_OSD_DATA, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_OSD_DATA2, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_USER_DATA, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_USER_DATAS, &shm);

    ret |= kmpp_meta_get_shm(meta, KEY_QPMAP0, &shm);
    ret |= kmpp_meta_get_shm(meta, KEY_NPU_SOBJ_FLAG, &shm);
    ret |= kmpp_meta_get_ptr(meta, KEY_NPU_UOBJ_FLAG, &ptr);

    ret |= kmpp_meta_get_s32(meta, KEY_ENC_MARK_LTR, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_ENC_USE_LTR, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_ENC_FRAME_QP, &val);
    ret |= kmpp_meta_get_s32(meta, KEY_ENC_BASE_LAYER_PID, &val);

    return ret;
}

void *meta_test(void *param)
{
    MetaTestCtx *ctx = (MetaTestCtx *)param;
    RK_S32 loop_max = ctx->loop_cnt;
    RK_S64 time_start;
    RK_S64 time_end;
    MPP_RET ret = MPP_OK;
    RK_S32 i;

    time_start = mpp_time();

    for (i = 0; i < loop_max; i++) {
        KmppMeta meta = NULL;

        ret |= kmpp_meta_get_f(&meta);
        mpp_assert(meta);

        /* set */
        ret |= meta_set(meta);
        /* get */
        ret |= meta_get(meta);

        ret |= kmpp_meta_put_f(meta);
    }

    time_end = mpp_time();

    if (ret)
        mpp_log("meta setting and getting, ret %d\n", ret);

    ctx->time_avg = (time_end - time_start) / loop_max;

    return NULL;
}

int main(int argc, char **argv)
{
    MetaTestCtx *ctxs = NULL;
    RK_S32 loop_cnt = 0;
    RK_S32 thd_cnt = 0;
    RK_S32 created = 0;
    RK_S64 avg_time = 0;
    MPP_RET ret = MPP_OK;
    RK_S32 i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-loop") && i + 1 < argc)
            loop_cnt = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-threads") && i + 1 < argc)
            thd_cnt = atoi(argv[++i]);
    }

    if (loop_cnt <= 0)
        loop_cnt = LOOP_DEFAULT;
    if (thd_cnt <= 0)
        thd_cnt = THRD_DEFAULT;

    ctxs = mpp_calloc(MetaTestCtx, thd_cnt);
    if (!ctxs) {
        mpp_log(MODULE_TAG " alloc failed\n");
        ret = MPP_NOK;
        goto done;
    }

    mpp_log(MODULE_TAG " start threads %d loop %d\n", thd_cnt, loop_cnt);

    for (i = 0; i < thd_cnt; i++) {
        ctxs[i].loop_cnt = loop_cnt;
        ctxs[i].thread = mpp_thread_create(meta_test, &ctxs[i], "meta_test");
        if (!ctxs[i].thread) {
            mpp_log(MODULE_TAG " thread %d create failed\n", i);
            ret = MPP_NOK;
            goto done;
        }
        mpp_thread_start(ctxs[i].thread);
        created++;
    }

done:
    /* join + destroy all successfully created threads */
    for (i = 0; i < created; i++) {
        mpp_thread_destroy(ctxs[i].thread);
        ctxs[i].thread = NULL;
    }

    if (ret == MPP_OK) {
        for (i = 0; i < created; i++)
            avg_time += ctxs[i].time_avg;
        mpp_log(MODULE_TAG " %d threads %d loop config avg %lld us",
                created, loop_cnt, created > 0 ? avg_time / created : 0);
        mpp_log(MODULE_TAG " done\n");
    }

    MPP_FREE(ctxs);

    return ret;
}
