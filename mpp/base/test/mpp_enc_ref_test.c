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
#include "mpp_mem.h"
#include "mpp_test.h"

#include "rk_venc_ref.h"
#include "kmpp_obj.h"
#include "mpp_cfg_io.h"
#include "mpp_enc_ref.h"
#include "mpp_internal.h"

extern KmppObjDef mpp_enc_ref_cfg_objdef(void);

/*
 * test_objdef_access - verify objdef trie entries (metadata only)
 *
 * Scalar entries and array subroots are queried directly.
 * VLA field access uses kmpp_obj_pos_seek + kmpp_obj_pos_get_s32.
 */
static rk_s32 test_objdef_access(void)
{
    KmppObjDef def = mpp_enc_ref_cfg_objdef();
    KmppEntry *entry = NULL;
    rk_s32 ret;
    rk_s32 i;

    mpp_logi("test_objdef_access start\n");

    if (!def) {
        mpp_loge("objdef is NULL\n");
        return rk_nok;
    }

    /* 1. scalar entries */
    struct { const char *name; ElemType type; } scalars[] = {
        { "keep_cpb",    ELEM_TYPE_s32 },
        { "lt_cfg_cnt",  ELEM_TYPE_s32 },
        { "st_cfg_off",  ELEM_TYPE_u32 },
        { "nonexistent", ELEM_TYPE_BUTT },
    };

    for (i = 0; i < (rk_s32)MPP_ARRAY_ELEMS(scalars); i++) {
        ret = kmpp_objdef_get_entry(def, scalars[i].name, &entry);
        if (scalars[i].type != ELEM_TYPE_BUTT) {
            if (ret || !entry || entry->tbl.elem_type != scalars[i].type) {
                mpp_loge("scalar '%s' failed\n", scalars[i].name);
                return rk_nok;
            }
        } else {
            if (!ret && entry) {
                mpp_loge("'%s' should not exist\n", scalars[i].name);
                return rk_nok;
            }
        }
    }

    /* 2. array subroots */
    struct { const char *name; rk_u32 expect_size; } arrays[] = {
        { "st_cfg", (rk_u32)sizeof(MppEncRefStFrmCfg) },
        { "lt_cfg", (rk_u32)sizeof(MppEncRefLtFrmCfg) },
    };

    for (i = 0; i < (rk_s32)MPP_ARRAY_ELEMS(arrays); i++) {
        ret = kmpp_objdef_get_entry(def, arrays[i].name, &entry);
        if (ret || !entry || entry->vla.type != ENTRY_TYPE_VLA_INFO ||
            entry->vla.elem_size != arrays[i].expect_size) {
            mpp_loge("array '%s' failed\n", arrays[i].name);
            return rk_nok;
        }
    }

    /* 3. negative test: paths without index are invalid */
    const char *bad_paths[] = {
        "st_cfg:is_non_ref", "lt_cfg:lt_idx",
    };

    for (i = 0; i < (rk_s32)MPP_ARRAY_ELEMS(bad_paths); i++) {
        ret = kmpp_objdef_get_entry(def, bad_paths[i], &entry);
        if (!ret) {
            mpp_loge("'%s' should fail (no index)\n", bad_paths[i]);
            return rk_nok;
        }
    }

    mpp_logi("test_objdef_access success\n");
    return rk_ok;
}

/*
 * test_obj_access - verify VLA data access on obj instance
 *
 * Uses kmpp_obj_pos for field navigation,
 * uses MPP_REF_ST_ARR/LT_ARR for array base address.
 */
static rk_s32 test_obj_access(void)
{
    MPP_RET_VARS;
    KmppObjDef def = mpp_enc_ref_cfg_objdef();
    KmppEntry *st_cfg_arr = NULL;
    KmppEntry *lt_cfg_arr = NULL;
    MppEncRefCfg obj = NULL;
    MppEncRefCfgImpl *cfg;
    KmppObjPos pos;
    rk_s32 val;

    mpp_logi("test_obj_access start\n");

    /* get array entries from objdef for stride */
    kmpp_objdef_get_entry(def, "st_cfg", &st_cfg_arr);
    kmpp_objdef_get_entry(def, "lt_cfg", &lt_cfg_arr);
    MPP_ASSERT_NOT_NULL(st_cfg_arr);
    MPP_ASSERT_NOT_NULL(lt_cfg_arr);

    /* create obj with known data */
    _mpp_ret = mpp_enc_ref_cfg_setup(&obj, 1, 4);
    MPP_ASSERT_FALSEm("setup failed", _mpp_ret);

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj);
    MppEncRefStFrmCfg *st_arr = MPP_REF_ST_ARR(cfg);

    st_arr[0].is_non_ref   = 0;
    st_arr[0].temporal_id  = 0;
    st_arr[0].ref_arg      = 10;
    st_arr[1].is_non_ref   = 1;
    st_arr[1].temporal_id  = 3;
    st_arr[1].ref_arg      = 20;
    st_arr[2].is_non_ref   = 0;
    st_arr[2].temporal_id  = 1;
    st_arr[2].ref_arg      = 30;
    st_arr[3].is_non_ref   = 1;
    st_arr[3].temporal_id  = 2;
    st_arr[3].ref_arg      = 40;
    cfg->st_cfg_cnt = 4;

    MppEncRefLtFrmCfg *lt_arr = MPP_REF_LT_ARR(cfg);
    lt_arr[0].lt_idx = 2;
    lt_arr[0].lt_gap = 100;
    cfg->lt_cfg_cnt = 1;

    /* st_cfg:0:is_non_ref -> 0 */
    kmpp_obj_pos_init(&pos);
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, "st_cfg", 0);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 0", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "is_non_ref", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:0:is_non_ref", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:0:is_non_ref", 0, val);

    /* st_cfg:1:is_non_ref -> 1 */
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, NULL, 1);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 1", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "is_non_ref", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:1:is_non_ref", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:1:is_non_ref", 1, val);

    /* st_cfg:2:temporal_id -> 1 */
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, NULL, 2);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 2", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "temporal_id", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:2:temporal_id", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:2:temporal_id", 1, val);

    /* st_cfg:3:ref_arg -> 40 */
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, NULL, 3);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 3", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "ref_arg", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:3:ref_arg", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:3:ref_arg", 40, val);

    /* lt_cfg:0:lt_idx -> 2 */
    kmpp_obj_pos_init(&pos);
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, "lt_cfg", 0);
    MPP_ASSERT_FALSEm("pos_seek lt_cfg 0", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "lt_idx", &val);
    MPP_ASSERT_FALSEm("pos_get lt_cfg:0:lt_idx", _mpp_ret);
    MPP_ASSERT_EQm("lt_cfg:0:lt_idx", 2, val);

    /* lt_cfg:0:lt_gap -> 100 */
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "lt_gap", &val);
    MPP_ASSERT_FALSEm("pos_get lt_cfg:0:lt_gap", _mpp_ret);
    MPP_ASSERT_EQm("lt_cfg:0:lt_gap", 100, val);

    /* st_cfg:4:is_non_ref -> out of range (should fail) */
    kmpp_obj_pos_init(&pos);
    {
        rk_s32 ret_seek = kmpp_obj_pos_seek(obj, &pos, "st_cfg", 4);
        MPP_ASSERTm("pos_seek st_cfg 4 should fail (out of range)", ret_seek);
    }

    mpp_logi("test_obj_access success\n");
    MPP_PASS();

done:
    if (obj)
        mpp_enc_ref_cfg_deinit(&obj);
    return _mpp_ret;
}

static rk_s32 test_vla_api(void)
{
    MPP_RET_VARS;
    MppEncRefCfg obj = NULL;
    MppEncRefCfgImpl *cfg;
    MppEncRefStFrmCfg *st_arr;
    MppEncRefLtFrmCfg *lt_arr;
    KmppObjPos pos;
    rk_s32 val;

    mpp_logi("test_vla_api start\n");

    /* create obj with known data */
    _mpp_ret = mpp_enc_ref_cfg_setup(&obj, 1, 4);
    MPP_ASSERT_FALSEm("setup failed", _mpp_ret);

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj);
    st_arr = MPP_REF_ST_ARR(cfg);
    st_arr[0].is_non_ref   = 0;
    st_arr[0].temporal_id  = 0;
    st_arr[0].ref_arg      = 10;
    st_arr[1].is_non_ref   = 1;
    st_arr[1].temporal_id  = 3;
    st_arr[1].ref_arg      = 20;
    st_arr[2].is_non_ref   = 0;
    st_arr[2].temporal_id  = 1;
    st_arr[2].ref_arg      = 30;
    cfg->st_cfg_cnt = 3;

    lt_arr = MPP_REF_LT_ARR(cfg);
    lt_arr[0].lt_idx = 5;
    lt_arr[0].lt_gap = 200;
    cfg->lt_cfg_cnt = 1;

    /* 1. get st_cfg:0:is_non_ref -> 0 */
    kmpp_obj_pos_init(&pos);
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, "st_cfg", 0);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 0", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "is_non_ref", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:0:is_non_ref", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:0:is_non_ref", 0, val);

    /* 2. get + set + readback st_cfg:1:is_non_ref */
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, NULL, 1);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 1", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "is_non_ref", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:1:is_non_ref", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:1:is_non_ref", 1, val);
    _mpp_ret = kmpp_obj_pos_set_s32(obj, &pos, "is_non_ref", 42);
    MPP_ASSERT_FALSEm("pos_set st_cfg:1:is_non_ref", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "is_non_ref", &val);
    MPP_ASSERT_FALSEm("pos_get after set", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:1:is_non_ref after set", 42, val);

    /* 3. st_cfg:2:temporal_id -> 1 */
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, NULL, 2);
    MPP_ASSERT_FALSEm("pos_seek st_cfg 2", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "temporal_id", &val);
    MPP_ASSERT_FALSEm("pos_get st_cfg:2:temporal_id", _mpp_ret);
    MPP_ASSERT_EQm("st_cfg:2:temporal_id", 1, val);

    /* 4. lt_cfg:0:lt_idx -> 5 */
    kmpp_obj_pos_init(&pos);
    _mpp_ret = kmpp_obj_pos_seek(obj, &pos, "lt_cfg", 0);
    MPP_ASSERT_FALSEm("pos_seek lt_cfg 0", _mpp_ret);
    _mpp_ret = kmpp_obj_pos_get_s32(obj, &pos, "lt_idx", &val);
    MPP_ASSERT_FALSEm("pos_get lt_cfg:0:lt_idx", _mpp_ret);
    MPP_ASSERT_EQm("lt_cfg:0:lt_idx", 5, val);

    /* 5. out of range: st_cfg, cap=4 (should fail) */
    kmpp_obj_pos_init(&pos);
    {
        rk_s32 ret_seek = kmpp_obj_pos_seek(obj, &pos, "st_cfg", 4);
        MPP_ASSERTm("pos_seek st_cfg 4 should fail (out of range)", ret_seek);
    }

    mpp_logi("test_vla_api success\n");
    MPP_PASS();

done:
    if (obj)
        mpp_enc_ref_cfg_deinit(&obj);
    return _mpp_ret;
}

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

    ret = mpp_enc_ref_cfg_dump(ref, __FUNCTION__);

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

/*
 * test_cfg_roundtrip - verify extract + apply roundtrip
 *
 * 1. Create obj with known scalar and VLA data
 * 2. Extract: struct -> cfg tree -> JSON string
 * 3. Apply: JSON string -> cfg tree -> new struct
 * 4. Verify scalar values match
 */
static rk_s32 test_cfg_roundtrip(void)
{
    MPP_RET_VARS;
    MppEncRefCfg obj = NULL;
    MppEncRefCfg obj2 = NULL;
    MppEncRefCfg obj3 = NULL;
    KmppObjDef def = mpp_enc_ref_cfg_objdef();
    MppCfgObj cfg_root = kmpp_objdef_get_cfg_root(def);
    MppCfgObj obj_from = NULL;
    MppCfgObj obj_from_json = NULL;
    MppEncRefCfgImpl *cfg;
    MppEncRefCfgImpl *cfg2;
    MppEncRefCfgImpl *cfg3;
    char *json = NULL;

    mpp_logi("test_cfg_roundtrip start\n");

    /* 1. create obj with known data */
    _mpp_ret = mpp_enc_ref_cfg_setup(&obj, 1, 4);
    MPP_ASSERT_FALSEm("setup failed", _mpp_ret);

    cfg = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj);
    cfg->keep_cpb = 1;

    MppEncRefStFrmCfg *st_arr = MPP_REF_ST_ARR(cfg);
    st_arr[0].is_non_ref   = 0;
    st_arr[0].temporal_id  = 0;
    st_arr[0].ref_arg      = 10;
    st_arr[1].is_non_ref   = 1;
    st_arr[1].temporal_id  = 3;
    st_arr[1].ref_arg      = 20;
    st_arr[2].is_non_ref   = 0;
    st_arr[2].temporal_id  = 1;
    st_arr[2].ref_arg      = 30;
    st_arr[3].is_non_ref   = 1;
    st_arr[3].temporal_id  = 2;
    st_arr[3].ref_arg      = 40;
    cfg->st_cfg_cnt = 4;

    MppEncRefLtFrmCfg *lt_arr = MPP_REF_LT_ARR(cfg);
    lt_arr[0].lt_idx = 2;
    lt_arr[0].lt_gap = 100;
    cfg->lt_cfg_cnt = 1;

    /* 2. extract: struct -> cfg tree -> JSON */
    _mpp_ret = mpp_cfg_from_struct(&obj_from, cfg_root, cfg);
    MPP_ASSERT_FALSEm("from_struct failed", _mpp_ret);

    _mpp_ret = mpp_cfg_to_string(obj_from, MPP_CFG_STR_FMT_JSON, &json);
    MPP_ASSERT_FALSEm("to_string failed", _mpp_ret);
    mpp_logi("extracted JSON:\n%s\n", json);

    /* 3. apply: extracted cfg -> new struct */
    {
        MppEncRefStFrmCfg *st_arr2;
        MppEncRefLtFrmCfg *lt_arr2;

        _mpp_ret = mpp_enc_ref_cfg_setup(&obj2, 1, 4);
        MPP_ASSERT_FALSEm("setup obj2 failed", _mpp_ret);

        cfg2 = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj2);

        _mpp_ret = mpp_cfg_to_struct(obj_from, cfg_root, cfg2);
        MPP_ASSERT_FALSEm("to_struct failed", _mpp_ret);

        /* verify scalars */
        MPP_ASSERT_EQ(1, cfg2->keep_cpb);
        MPP_ASSERT_EQ(4, cfg2->st_cfg_cnt);
        MPP_ASSERT_EQ(1, cfg2->lt_cfg_cnt);

        /* verify VLA fields */
        st_arr2 = MPP_REF_ST_ARR(cfg2);
        MPP_ASSERT_EQ(0, st_arr2[0].is_non_ref);
        MPP_ASSERT_EQ(10, st_arr2[0].ref_arg);
        MPP_ASSERT_EQ(1, st_arr2[1].is_non_ref);
        MPP_ASSERT_EQ(3, st_arr2[1].temporal_id);
        MPP_ASSERT_EQ(20, st_arr2[1].ref_arg);
        MPP_ASSERT_EQ(0, st_arr2[2].is_non_ref);
        MPP_ASSERT_EQ(1, st_arr2[2].temporal_id);
        MPP_ASSERT_EQ(30, st_arr2[2].ref_arg);
        MPP_ASSERT_EQ(1, st_arr2[3].is_non_ref);
        MPP_ASSERT_EQ(2, st_arr2[3].temporal_id);
        MPP_ASSERT_EQ(40, st_arr2[3].ref_arg);

        lt_arr2 = MPP_REF_LT_ARR(cfg2);
        MPP_ASSERT_EQ(2, lt_arr2[0].lt_idx);
        MPP_ASSERT_EQ(100, lt_arr2[0].lt_gap);
    }

    /* 4. JSON roundtrip: JSON -> cfg tree -> new struct -> compare */
    {
        MppEncRefStFrmCfg *st_arr3;
        MppEncRefLtFrmCfg *lt_arr3;

        _mpp_ret = mpp_cfg_from_string(&obj_from_json, MPP_CFG_STR_FMT_JSON, json);
        MPP_ASSERT_FALSEm("from_string failed", _mpp_ret);

        _mpp_ret = mpp_enc_ref_cfg_setup(&obj3, 1, 4);
        MPP_ASSERT_FALSEm("setup obj3 failed", _mpp_ret);

        cfg3 = (MppEncRefCfgImpl *)kmpp_obj_to_entry(obj3);

        _mpp_ret = mpp_cfg_to_struct(obj_from_json, cfg_root, cfg3);
        MPP_ASSERT_FALSEm("to_struct from json failed", _mpp_ret);

        /* verify scalars */
        MPP_ASSERT_EQ(1, cfg3->keep_cpb);
        MPP_ASSERT_EQ(4, cfg3->st_cfg_cnt);
        MPP_ASSERT_EQ(1, cfg3->lt_cfg_cnt);

        /* verify VLA fields */
        st_arr3 = MPP_REF_ST_ARR(cfg3);
        MPP_ASSERT_EQ(0, st_arr3[0].is_non_ref);
        MPP_ASSERT_EQ(10, st_arr3[0].ref_arg);
        MPP_ASSERT_EQ(1, st_arr3[1].is_non_ref);
        MPP_ASSERT_EQ(3, st_arr3[1].temporal_id);
        MPP_ASSERT_EQ(20, st_arr3[1].ref_arg);
        MPP_ASSERT_EQ(0, st_arr3[2].is_non_ref);
        MPP_ASSERT_EQ(1, st_arr3[2].temporal_id);
        MPP_ASSERT_EQ(30, st_arr3[2].ref_arg);
        MPP_ASSERT_EQ(1, st_arr3[3].is_non_ref);
        MPP_ASSERT_EQ(2, st_arr3[3].temporal_id);
        MPP_ASSERT_EQ(40, st_arr3[3].ref_arg);

        lt_arr3 = MPP_REF_LT_ARR(cfg3);
        MPP_ASSERT_EQ(2, lt_arr3[0].lt_idx);
        MPP_ASSERT_EQ(100, lt_arr3[0].lt_gap);
    }

    mpp_logi("test_cfg_roundtrip success\n");
    MPP_PASS();

done:
    MPP_FREE(json);
    if (obj_from)
        mpp_cfg_put_all(obj_from);
    if (obj_from_json)
        mpp_cfg_put_all(obj_from_json);
    if (obj)
        mpp_enc_ref_cfg_deinit(&obj);
    if (obj2)
        mpp_enc_ref_cfg_deinit(&obj2);
    if (obj3)
        mpp_enc_ref_cfg_deinit(&obj3);
    return _mpp_ret;
}

int main(void)
{
    MPP_RET ret = MPP_OK;

    mpp_logi("mpp_enc_ref_test start\n");

    /* show objdef entries first (like mpp_enc_cfg_test) */
    mpp_enc_ref_cfg_show();

    /* verify cfg tree includes VLA arrays with their fields */
    {
        KmppObjDef def = mpp_enc_ref_cfg_objdef();
        MppCfgObj cfg_root = kmpp_objdef_get_cfg_root(def);

        if (cfg_root) {
            mpp_logi("cfg tree dump:\n");
            mpp_cfg_dump(cfg_root, "ref_cfg");
        } else {
            mpp_loge("cfg root is NULL\n");
            ret = MPP_NOK;
            goto done;
        }
    }

    ret = test_objdef_access();
    if (ret)
        goto done;

    ret = test_obj_access();
    if (ret)
        goto done;

    ret = test_vla_api();
    if (ret)
        goto done;

    ret = test_mpp_enc_ref_cfg_obj();
    if (ret)
        goto done;

    ret = test_mpp_enc_ref_cfg_copy_shrink();
    if (ret)
        goto done;

    ret = test_cfg_roundtrip();
    if (ret)
        goto done;

    ret = test_mpp_enc_ref_cfg_tsvc4();

done:
    mpp_logi("mpp_enc_ref_test %s\n", ret ? "failed" : "success");
    return ret;
}