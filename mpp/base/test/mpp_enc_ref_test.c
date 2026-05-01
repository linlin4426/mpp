/*
 * Copyright 2015 Rockchip Electronics Co. LTD
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

#define MODULE_TAG "mpp_enc_ref_test"

#include <string.h>

#include "mpp_log.h"
#include "mpp_common.h"

#include "rk_venc_ref.h"
#include "kmpp_obj.h"
#include "mpp_enc_ref.h"

/*
 * test_copy_shrink: verify copy from small src to large dst does not overflow.
 * Without the cnt=0 fix in copy, resize callback memmove would overflow when
 * dst has more lt entries than src's lt_cap.
 */
static rk_s32 test_mpp_enc_ref_cfg_copy_shrink(void)
{
    MppEncRefCfg src = NULL;
    MppEncRefCfg dst = NULL;
    MppEncRefCfgImpl *cfg;
    MppEncRefLtFrmCfg lt_ref;
    MppEncRefStFrmCfg st_ref;
    rk_s32 ret;

    mpp_logi("test_mpp_enc_ref_cfg_copy_shrink start\n");

    /* src: small config - 1 lt, 1 st */
    ret = mpp_enc_ref_cfg_setup(&src, 1, 1);
    if (ret) {
        mpp_loge("setup src failed ret %d\n", ret);
        goto done;
    }

    memset(&lt_ref, 0, sizeof(lt_ref));
    lt_ref.lt_idx      = 0;
    lt_ref.ref_mode    = REF_TO_PREV_LT_REF;
    ret = mpp_enc_ref_cfg_add_lt_cfg(src, 1, &lt_ref);

    memset(&st_ref, 0, sizeof(st_ref));
    st_ref.ref_mode    = REF_TO_PREV_REF_FRM;
    ret = mpp_enc_ref_cfg_add_st_cfg(src, 1, &st_ref);

    /* dst: large config - 8 lt, 8 st, all filled */
    ret = mpp_enc_ref_cfg_setup(&dst, 8, 8);
    if (ret) {
        mpp_loge("setup dst failed ret %d\n", ret);
        goto done;
    }

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(dst);
    mpp_logi("dst before copy: lt_cap %d lt_cnt %d\n",
             cfg->lt_cfg_cap, cfg->lt_cfg_cnt);

    memset(&lt_ref, 0, sizeof(lt_ref));
    lt_ref.lt_idx      = 0;
    lt_ref.ref_mode    = REF_TO_PREV_LT_REF;
    for (rk_s32 i = 0; i < 8; i++) {
        lt_ref.lt_idx = i;
        mpp_enc_ref_cfg_add_lt_cfg(dst, 1, &lt_ref);
    }

    memset(&st_ref, 0, sizeof(st_ref));
    st_ref.ref_mode = REF_TO_PREV_REF_FRM;
    for (rk_s32 i = 0; i < 8; i++)
        mpp_enc_ref_cfg_add_st_cfg(dst, 1, &st_ref);

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(dst);
    mpp_logi("dst filled: lt_cap %d lt_cnt %d st_cap %d st_cnt %d\n",
             cfg->lt_cfg_cap, cfg->lt_cfg_cnt, cfg->st_cfg_cap, cfg->st_cfg_cnt);

    /* copy small src to large dst - shrinks dst from 8+8 to 1+1 */
    ret = mpp_enc_ref_cfg_copy(dst, src);
    if (ret) {
        mpp_loge("copy shrink failed ret %d\n", ret);
        goto done;
    }

    /* verify dst now matches src */
    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(dst);
    if (cfg->lt_cfg_cap != 1 || cfg->st_cfg_cap != 1) {
        mpp_loge("copy shrink cap mismatch: lt %d st %d\n",
                 cfg->lt_cfg_cap, cfg->st_cfg_cap);
        ret = rk_nok;
        goto done;
    }
    if (cfg->lt_cfg_cnt != 1 || cfg->st_cfg_cnt != 1) {
        mpp_loge("copy shrink cnt mismatch: lt %d st %d\n",
                 cfg->lt_cfg_cnt, cfg->st_cfg_cnt);
        ret = rk_nok;
        goto done;
    }

    mpp_logi("test_mpp_enc_ref_cfg_copy_shrink success\n");

done:
    if (src)
        mpp_enc_ref_cfg_deinit(&src);
    if (dst)
        mpp_enc_ref_cfg_deinit(&dst);

    return ret;
}

static rk_s32 test_mpp_enc_ref_cfg_tsvc4(void)
{
    MppEncRefCfg ref = NULL;
    MppEncRefLtFrmCfg lt_ref[4];
    MppEncRefStFrmCfg st_ref[16];
    rk_s32 ret;

    mpp_logi("test_mpp_enc_ref_cfg_tsvc4 start\n");

    memset(&lt_ref, 0, sizeof(lt_ref));
    memset(&st_ref, 0, sizeof(st_ref));

    ret = mpp_enc_ref_cfg_init(&ref);

    ret = mpp_enc_ref_cfg_set_cfg_cnt(ref, 1, 9);

    /* set 8 frame lt-ref gap */
    lt_ref[0].lt_idx        = 0;
    lt_ref[0].temporal_id   = 0;
    lt_ref[0].ref_mode      = REF_TO_PREV_LT_REF;
    lt_ref[0].lt_gap        = 8;
    lt_ref[0].lt_delay      = 0;

    ret = mpp_enc_ref_cfg_add_lt_cfg(ref, 1, lt_ref);

    /* set tsvc4 st-ref struct */
    /* st 0 layer 0 - ref */
    st_ref[0].is_non_ref    = 0;
    st_ref[0].temporal_id   = 0;
    st_ref[0].ref_mode      = REF_TO_TEMPORAL_LAYER;
    st_ref[0].ref_arg       = 0;
    st_ref[0].repeat        = 0;
    /* st 1 layer 3 - non-ref */
    st_ref[1].is_non_ref    = 1;
    st_ref[1].temporal_id   = 3;
    st_ref[1].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[1].ref_arg       = 0;
    st_ref[1].repeat        = 0;
    /* st 2 layer 2 - ref */
    st_ref[2].is_non_ref    = 0;
    st_ref[2].temporal_id   = 2;
    st_ref[2].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[2].ref_arg       = 0;
    st_ref[2].repeat        = 0;
    /* st 3 layer 3 - non-ref */
    st_ref[3].is_non_ref    = 1;
    st_ref[3].temporal_id   = 3;
    st_ref[3].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[3].ref_arg       = 0;
    st_ref[3].repeat        = 0;
    /* st 4 layer 1 - ref */
    st_ref[4].is_non_ref    = 0;
    st_ref[4].temporal_id   = 1;
    st_ref[4].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[4].ref_arg       = 0;
    st_ref[4].repeat        = 0;
    /* st 5 layer 3 - non-ref */
    st_ref[5].is_non_ref    = 1;
    st_ref[5].temporal_id   = 3;
    st_ref[5].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[5].ref_arg       = 0;
    st_ref[5].repeat        = 0;
    /* st 6 layer 2 - ref */
    st_ref[6].is_non_ref    = 0;
    st_ref[6].temporal_id   = 2;
    st_ref[6].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[6].ref_arg       = 0;
    st_ref[6].repeat        = 0;
    /* st 7 layer 3 - non-ref */
    st_ref[7].is_non_ref    = 1;
    st_ref[7].temporal_id   = 3;
    st_ref[7].ref_mode      = REF_TO_PREV_REF_FRM;
    st_ref[7].ref_arg       = 0;
    st_ref[7].repeat        = 0;
    /* st 8 layer 0 - ref */
    st_ref[8].is_non_ref    = 0;
    st_ref[8].temporal_id   = 0;
    st_ref[8].ref_mode      = REF_TO_TEMPORAL_LAYER;
    st_ref[8].ref_arg       = 0;
    st_ref[8].repeat        = 0;

    ret = mpp_enc_ref_cfg_add_st_cfg(ref, 9, st_ref);

    ret = mpp_enc_ref_cfg_check(ref);
    mpp_logi("test_mpp_enc_ref_cfg_tsvc4 check ret %d\n", ret);

    ret = mpp_enc_ref_cfg_show(ref);

    mpp_enc_ref_cfg_deinit(&ref);

    mpp_logi("test_mpp_enc_ref_cfg_tsvc4 %s\n", ret ? "failed" : "success");
    return ret;
}

static rk_s32 test_mpp_enc_ref_cfg_obj(void)
{
    MppEncRefCfg obj = NULL;
    MppEncRefCfgImpl *cfg;
    MppEncRefStFrmCfg *st_arr;
    MppEncRefLtFrmCfg *lt_arr;
    rk_s32 lt_cnt = 1;
    rk_s32 st_cnt = 9;
    rk_s32 ret;
    rk_s32 i;

    mpp_logi("test_mpp_enc_ref_cfg_obj start\n");

    /* test 1: tsvc4 layout (1 lt + 9 st) */
    ret = mpp_enc_ref_cfg_setup(&obj, lt_cnt, st_cnt);
    if (ret) {
        mpp_loge("ref_cfg_obj_init failed ret %d\n", ret);
        goto done;
    }

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj);
    mpp_logi("ref_cfg_obj: lt_cfg_cap %d st_cfg_cap %d st_cfg_off %u lt_cfg_off %u\n",
             cfg->lt_cfg_cap, cfg->st_cfg_cap, cfg->st_cfg_off, cfg->lt_cfg_off);

    /* verify offsets */
    if (cfg->st_cfg_cap != st_cnt || cfg->lt_cfg_cap != lt_cnt) {
        mpp_loge("ref_cfg_obj cap mismatch: lt %d/%d st %d/%d\n",
                 cfg->lt_cfg_cap, lt_cnt, cfg->st_cfg_cap, st_cnt);
        ret = rk_nok;
        goto done;
    }

    if (cfg->lt_cfg_off != cfg->st_cfg_off + st_cnt * sizeof(MppEncRefStFrmCfg)) {
        mpp_loge("ref_cfg_obj offset mismatch: lt_cfg_off %u expected %u\n",
                 cfg->lt_cfg_off, cfg->st_cfg_off + (rk_u32)(st_cnt * sizeof(MppEncRefStFrmCfg)));
        ret = rk_nok;
        goto done;
    }

    /* write and verify st_cfg */
    st_arr = MPP_REF_ST_ARR(cfg);
    for (i = 0; i < st_cnt; i++) {
        st_arr[i].temporal_id = i % 4;
        st_arr[i].is_non_ref  = (i % 4 == 3) ? 1 : 0;
        st_arr[i].ref_mode    = REF_TO_PREV_REF_FRM;
        st_arr[i].ref_arg     = 0;
        st_arr[i].repeat      = 0;
    }

    for (i = 0; i < st_cnt; i++) {
        if (st_arr[i].temporal_id != i % 4 || st_arr[i].is_non_ref != ((i % 4 == 3) ? 1 : 0)) {
            mpp_loge("ref_cfg_obj st[%d] data mismatch\n", i);
            ret = rk_nok;
            goto done;
        }
    }

    /* write and verify lt_cfg */
    lt_arr = MPP_REF_LT_ARR(cfg);
    lt_arr[0].lt_idx      = 0;
    lt_arr[0].temporal_id = 0;
    lt_arr[0].ref_mode    = REF_TO_PREV_LT_REF;
    lt_arr[0].ref_arg     = 0;
    lt_arr[0].lt_gap      = 8;
    lt_arr[0].lt_delay    = 0;

    if (lt_arr[0].lt_gap != 8 || lt_arr[0].lt_idx != 0) {
        mpp_loge("ref_cfg_obj lt[0] data mismatch\n");
        ret = rk_nok;
        goto done;
    }

    mpp_logi("test ref_cfg_obj tsvc4 layout success\n");
    mpp_enc_ref_cfg_deinit(&obj);

    /* test 2: zero lt/st */
    ret = mpp_enc_ref_cfg_setup(&obj, 0, 0);
    if (ret) {
        mpp_loge("ref_cfg_obj_init(0,0) failed ret %d\n", ret);
        goto done;
    }

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj);
    if (cfg->st_cfg_off != cfg->lt_cfg_off) {
        mpp_loge("ref_cfg_obj(0,0) offset mismatch: st %u lt %u\n",
                 cfg->st_cfg_off, cfg->lt_cfg_off);
        ret = rk_nok;
        goto done;
    }

    mpp_logi("test ref_cfg_obj zero layout success\n");
    mpp_enc_ref_cfg_deinit(&obj);

    /* test 3: invalid params */
    ret = mpp_enc_ref_cfg_setup(NULL, 0, 0);
    if (!ret) {
        mpp_loge("ref_cfg_obj_init(NULL) should fail\n");
        ret = rk_nok;
        goto done;
    }

    ret = mpp_enc_ref_cfg_setup(&obj, -1, 0);
    if (!ret) {
        mpp_loge("ref_cfg_obj_init(-1,0) should fail\n");
        ret = rk_nok;
        goto done;
    }

    mpp_logi("test ref_cfg_obj invalid params success\n");
    ret = rk_ok;

done:
    if (obj)
        mpp_enc_ref_cfg_deinit(&obj);

    mpp_logi("test_mpp_enc_ref_cfg_obj %s\n", ret ? "failed" : "success");
    return ret;
}

int main(void)
{
    MPP_RET ret = MPP_OK;

    mpp_logi("mpp_enc_ref_test start\n");

    ret = test_mpp_enc_ref_cfg_obj();
    if (ret)
        goto done;

    ret = test_mpp_enc_ref_cfg_copy_shrink();
    if (ret)
        goto done;

    ret = test_mpp_enc_ref_cfg_tsvc4();

done:
    mpp_logi("mpp_enc_ref_test %s\n", ret ? "failed" : "success");
    return ret;
}