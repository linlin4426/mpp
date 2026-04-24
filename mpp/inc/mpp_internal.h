/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef MPP_INTERNAL_H
#define MPP_INTERNAL_H

#include "rk_type.h"
#include "mpp_err.h"

/*
 * All mpp / kmpp internal interface object list here.
 */
typedef void* MppTrie;
typedef void* MppCfgObj;

/* MppObjDef - mpp object name size and access table trie definition */
typedef void* MppObjDef;
/* MppObj    - mpp object for string name access and function access */
typedef void* MppObj;
typedef void* MppIoc;

/* KmppObjDef - mpp kernel object name size and access table trie definition */
typedef void* KmppObjDef;
/* KmppObj    - mpp kernel object for string name access and function access */
typedef void* KmppObj;

/*
 * kernel - userspace transaction trie node ctx info (64 bit) definition
 *
 * +------+--------------------+---------------------------+
 * | 8bit |       24 bit       |          32 bit           |
 * +------+--------------------+---------------------------+
 *
 * bit 0~3 - 4-bit entry type (EntryType)
 * 0 - invalid entry
 * 1 - trie self info node
 * 2 - access location table node
 * 3 - ioctl cmd node
 *
 * bit 4~7 - 4-bit entry flag (EntryFlag) for different entry type
 */
typedef enum EntryType_e {
    ENTRY_TYPE_NONE     = 0x0,  /* invalid entry type */
    ENTRY_TYPE_VAL      = 0x1,  /* 32-bit value  */
    ENTRY_TYPE_STR      = 0x2,  /* string info property */
    ENTRY_TYPE_LOC_TBL  = 0x3,  /* entry location table */
    ENTRY_TYPE_VLA_INFO = 0x4,  /* entry location table */
    ENTRY_TYPE_BUTT,
} EntryType;

/*
 * 4-bit extention flag for different entry property
 * EntryValFlag     - for ENTRY_TYPE_VAL
 * EntryValFlag     - for ENTRY_TYPE_STR
 * EntryLocTblFlag  - for ENTRY_TYPE_LOC_TBL
 */
typedef enum EntryValFlag_e {
    /*
     * 0 - value is unsigned value
     * 1 - value is signed value
     */
    VALUE_SIGNED        = 0x1,
} EntryValFlag;

typedef enum EntryValUsage_e {
    VALUE_NORMAL        = 0x0,

    VALUE_TRIE          = 0x10,
    /* trie info value */
    VALUE_TRIE_INFO     = (VALUE_TRIE + 1),
    /* trie offset from the trie root */
    VALUE_TRIE_OFFSET   = (VALUE_TRIE + 2),

    /* ioctl cmd */
    VALUE_IOCTL_CMD     = 0x20,
} EntryValUsage;

typedef enum EntryStrFlag_e {
    STRING_NORMAL       = 0x0,
    /* string is trie self info */
    STRING_TRIE         = 0x1,
} EntryStrFlag;

typedef enum EntryLocTblFlag_e {
    /*
     * bit 1    - array sub-root node (array pattern :N:)
     */
    LOCTBL_ARRAY_SUBROOT = 0x1,
} EntryLocTblFlag;

typedef enum ElemType_e {
    /* commaon fix size value */
    ELEM_TYPE_FIX       = 0x0,
    ELEM_TYPE_s32       = (ELEM_TYPE_FIX + 0),
    ELEM_TYPE_u32       = (ELEM_TYPE_FIX + 1),
    ELEM_TYPE_s64       = (ELEM_TYPE_FIX + 2),
    ELEM_TYPE_u64       = (ELEM_TYPE_FIX + 3),
    /* pointer type stored by 64-bit */
    ELEM_TYPE_ptr       = (ELEM_TYPE_FIX + 4),
    /* value only structure */
    ELEM_TYPE_st        = (ELEM_TYPE_FIX + 5),

    /* kernel and userspace share data */
    ELEM_TYPE_SHARE     = 0x6,
    /* share memory between kernel and userspace */
    ELEM_TYPE_shm       = (ELEM_TYPE_SHARE + 0),

    /* array type data */
    ELEM_TYPE_arr       = 0x7,

    /* kernel access only data */
    ELEM_TYPE_KERNEL    = 0x8,
    /* kenrel object poineter */
    ELEM_TYPE_kobj      = (ELEM_TYPE_KERNEL + 0),
    /* kenrel normal data poineter */
    ELEM_TYPE_kptr      = (ELEM_TYPE_KERNEL + 1),
    /* kernel function poineter */
    ELEM_TYPE_kfp       = (ELEM_TYPE_KERNEL + 2),

    /* userspace access only data */
    ELEM_TYPE_USER      = 0xc,
    /* userspace object poineter */
    ELEM_TYPE_uobj      = (ELEM_TYPE_USER + 0),
    /* userspace normal data poineter */
    ELEM_TYPE_uptr      = (ELEM_TYPE_USER + 1),
    /* userspace function poineter */
    ELEM_TYPE_ufp       = (ELEM_TYPE_USER + 2),

    ELEM_TYPE_s8        = 0x11,
    ELEM_TYPE_u8        = 0x12,
    ELEM_TYPE_s16       = 0x13,
    ELEM_TYPE_u16       = 0x14,
    /* variable size data */
    ELEM_TYPE_var       = 0x15,
    ELEM_TYPE_BUTT      = 0x16,
} ElemType;

typedef enum EntryVLAFlag_e {
    VLAINFO_FLEX_BASE   = 0x1,
    VLAINFO_FLEX_COUNT  = 0x2,
} EntryVLAFlag;

typedef union KmppEntry_u {
    rk_u64                  val;
    union {
        EntryType           type            : 4;
        struct KmppEntryVal {
            EntryType       prop            : 4;
            EntryValFlag    flag            : 4;
            EntryValUsage   usage           : 8;
            rk_u32          reserve         : 16;
            rk_u32          val;
        } v;
        struct KmppEntryStr {
            EntryType       prop            : 4;
            EntryValFlag    flag            : 4;
            rk_u32          len             : 24;
            rk_u32          offset;
        } str;
        struct KmppEntryLocTbl {
            EntryType       type            : 4;    /* ENTRY_TYPE_LOC_TBL */
            EntryLocTblFlag tbl_flag        : 4;
            ElemType        elem_type       : 8;
            rk_u16          elem_size;
            rk_u16          elem_offset;
            rk_u16          flag_offset;
        } tbl;
        /*
         * helper struct for location table for Variable Length Array (VLA)
         * which its base offset and count offset is not fixed
         * used only on in subroot trie root node's info
         */
        struct KmppEntryVLAInfo {
            EntryType       type            : 4;    /* ENTRY_TYPE_LOC_TBL */
            EntryVLAFlag    vla_flag        : 4;    /* vla flag */
            rk_u16          elem_size       : 8;    /* size of each array element */
            rk_u16          elem_count;             /* count of array element (0 for dynamic) */
            rk_u16          count_off;              /* offset of real array count in struct */
            rk_u16          base_off;               /* offset of real array base offset in struct */
        } vla;
    };
} KmppEntry;

#endif /* MPP_INTERNAL_H */
