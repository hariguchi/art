/*
   Copyright (c) 2001-2015
   Yoichi Hariguchi. All rights reserved.

   The algorithm of ART is invented by Donald Knuth in 2000 while he was
   reviewing Yoichi's paper (http://www.hariguchi.org/art/smart.pdf).
   This is an ART implementation by Yoichi Hariguchi that supports
   arbitrary address length and multiple routing tables.
   This implementation includes a small part of Don Knuth's original ART
   implementation for IPv4.

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#ifndef __ipArt_h__
#define __ipArt_h__
#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "Types.h"
#include "dllist.h"



typedef enum {
    simpleTrie   = 0,
    pathCompTrie = 1,           /* trie with path compression */
} trieType;

typedef struct routeEnt {
    u8  dest[16];               /* upto IPv6 */
    int plen;                   /* prefix length */
    int level;                  /* for performance */
} routeEnt, *pRouteEnt;


typedef union tableEntry {
    routeEnt         *ent;
    union tableEntry *down;
    union {
        struct {
            s16 nRoutes;     /* number of routes in this subtable */
            s16 nSubtables;  /* number of subtabls in this subtable */
        };
        size_t count;           /* reference counter */
    };
    size_t level;               /* t[-1] holds level */
} tableEntry;
typedef tableEntry *subtable;


typedef struct {
    u8 sb;                  /* start byte from the beg. of address */
    u8 bo;                  /* bit offset from bit 7 */
    u8 sl;                  /* stride length */
    u8 tl;                  /* total stride length up to this level */
} strideInfo;


typedef struct pcSubtbls pcSubtbls;
struct pcSubtbls {
    subtable pst;
    int      idx;
};

typedef struct rtTable rtTable;
struct rtTable {
    tableEntry*  root;    /* pointer to root subtable */
    strideInfo*  psi;     /* array of stride information indexed by level */
    union {
        tableEntry** pEnt;/* array of table entry pointers */
        routeEnt**   pDef;/* array of subtable (trie node) default routes */
    };
    subtable*    pTbl;    /* array of subtable pointers (for deletion) */
    pcSubtbls*   pPcSt;   /* array of pcSubtables pointers (for deleteion) */
    s16          alen;    /* address length in bits for search */
    u16          len;     /* address length in bytes */
    s16          off;     /* -1 - byte2nPtrs(len) */
    u16          nLevels; /* number of levels */

    routeEnt* (*insert)(rtTable* p, routeEnt* r);
    bool (*delete)(rtTable* p, u8* pDest, int plen);
    routeEnt* (*findMatch)(rtTable *p, u8* pDest);
    routeEnt* (*findExactMatch)(rtTable *p, u8* pDest, int plen);

    int  nRoutes;           /* # of routes */
    int* nHeaps;            /* # of heaps at level `i' */
    int* nTransit;          /* # of transit heaps at level `i'  */
    u32  nSubtablesFreed;   /* # of freed subtables (for debugging) */
};

typedef struct rtArtWalkQnode rtArtWalkQnode;
struct rtArtWalkQnode
{
    dllNode  dlln;              /* doubly linked list node */
    subtable p;
};

typedef void (*rtFunc)(routeEnt*, void*);


#define isSubtable(p) ((p).count&1)
#define makeSubtable(p) (subtable)((size_t)(p)|1)
#define subtablePtr(p) ((tableEntry)((p).count&-2))


/*
 * API functions
 */
routeEnt* rtArtNewRoute(rtTable *pt);
void      rtArtFreeRoute(rtTable *pt, routeEnt *);
rtTable*  rtArtInit(int nLevels, s8* psl, int alen, trieType type);
rtTable*  rtArtPcInit(rtTable* pt);
routeEnt* rtArtFindMatch(rtTable *pt, u8* pDest);
routeEnt* rtArtPcFindMatch(rtTable *pt, u8* pDest);
routeEnt* rtArtPcFindMatch(rtTable *pt, u8* pDest);
routeEnt* rtArtFindMatchStat(rtTable* pt, u8* pDest);
routeEnt* rtArtPcFindMatchStat(rtTable* pt, u8* pDest);
routeEnt* rtArtInsertRoute(rtTable *pt, routeEnt *r);
routeEnt* rtArtPcInsertRoute(rtTable *pt, routeEnt *r);
bool      rtArtDeleteRoute(rtTable *pt, u8* pDest, int plen);
bool      rtArtPcDeleteRoute(rtTable *pt, u8* pDest, int plen);
void      rtArtWalkTable(rtTable* pt, subtable p, int index,
                         int thresh, rtFunc f, void* p2);
void      rtArtWalkTrie (rtTable* pt, subtable p, rtFunc f, void* p2);
void      rtArtCollectStats(rtTable* pt, subtable ps);


/*
 * Inline functions
 */


/**
 * @name  bitCmp8
 *
 * @brief Compare between arbitrary bits of a byte
 *        
 *
 * @param[in] d1  Data 1 (one byte) to be compared.
 * @param[in] d2  Data 2 (one byte) to be compared.
 * @param[in] st  First bit (0-7) to be compared.
 * @param[in] end Last bit (0-7) to be compared.
 *
 * @retval Negative integer If bit st:end of d1 < bit st:end of d2
 * @retval 0                If bit st:end of d1 == bit st:end of d2
 * @retval Positive integer If bit st:end d1 > bit st:end of d2
 */
static inline int
bitCmp8 (u8 p1, u8 p2, int st, int end)
{
    u8 mask;

    assert(end >= st);
    assert(st >= 0 && st < 8);
    assert(end >= 0 && end < 8);

    mask =  (((u8)(~0)) >> (8 - (end - st + 1))) << st;
    p1 &= mask;
    p2 &= mask;
    return (p1 - p2);
}

/**
 * @name  bitStrCmp
 *
 * @brief Compare two bit strings
 *        
 *
 * @param[in] p1  Pointer to bit string 1 to be compared.
 * @param[in] p2  Pointer to bit string 2 to be compared.
 * @param[in] st  Start bit to be compared (bit 0 is the 7th bit of p1[0].)
 * @param[in] end End bit to be compared (bit 0 is the 7th bit of p1[0].)
 * @param[out] idx Pointer to the array index where the comparison
 *                 finished. This pointer can be NULL.
 *
 * @retval Negative integer If st:end bits of p1 < st:end bits of p2
 * @retval 0                If st:end bits of p1 == st:end bits of p2
 * @retval Positive integer st:end bits of p1 > st:end bits of p2
 */
static inline int
bitStrCmp (u8* p1,u8* p2, int st, int end, int* idx)
{
    register int sb;            /* start bit */
    int *i;
    int ii;
    int rc;

    assert(st <= end);

    i = (idx) ? idx : &ii;

    /* compare p1[*i] and p2[*i]
     */
    *i = st >> 3;
    sb = *i << 3;
    st -= sb;
    st = 7 - st;                /* end bit */
    if (end < (sb + 8)) {
        end -= sb;
        end = 7 - end;          /* start bit */
        return bitCmp8(p1[*i], p2[*i], end, st);
    } else {
        /*
         * more bits to compare
         */
        rc = bitCmp8(p1[*i], p2[*i], 0, st);
        if (rc) {
            return rc;
        }
        for (++*i, sb += 8; end >= (sb + 8); ++*i, sb += 8) {
            if (p1[*i] != p2[*i]) {
                return (p1[*i] - p2[*i]);
            }
        }
    }
    st = 7 - (end - sb);
    return bitCmp8(p1[*i], p2[*i], st, 7);
}


/**
 * @name  bytes2nPtrs
 *
 * @brief Convert number of bytes to the number of pointers
 *        This function works both on 32-bit and 64-bit CPUs.
 *
 * @param[in] nBytes Number of bytes
 *
 * @retval u32 The minimum number of pointers to cover `nBytes'
 */
static inline u32
bytes2nPtrs (u32 nBytes)
{
#if __SIZEOF_POINTER__ == 8
    return ((nBytes & 7) == 0) ? (nBytes >> 3) : ((nBytes >> 3) + 1);
#elif __SIZEOF_POINTER__ == 4
    return ((nBytes & 3) == 0) ? (nBytes >> 2) : ((nBytes >> 2) + 1);
#else
#error Pointer size must be 4 or 8.
#endif
}


/**
 * @name  bits2bytes
 *
 * @brief Convert number of bits to the number of bytes
 *
 * @param[in] nBits Number of bits
 *
 * @retval u32 The minimum number of bytes to cover `nBits'
 */
static inline u32
bits2bytes (u32 nBits)
{
    register u32 rv = nBits >> 3;
    if ((nBits & 7)) {
        return (rv + 1);
    }
    return rv;
}


/**
 * @name  cmpAddr
 *
 * @brief Compare two bit strings for the first 'plen' bits
 *
 * @param[in] p1   Pointer to bit string 1 to be compared.
 * @param[in] p2   Pointer to bit string 2 to be compared.
 * @param[in] plen Number of bits to be compared from the beginning
 *
 * @retval true  Two bit strings are identical
 * @retval false Two bit strings are different
 */
static inline bool
cmpAddr (u8* p1, u8* p2, int plen)
{
    int len;
    u8  mask;


    for ( len = 8; len <= plen; len += 8 ) {
        if ( *p1++ != *p2++ ) {
            return false;
        }
    }
    if ( len == plen ) {
        return true;
    }

    /*
     * check remining bits < 8
     */
    mask = ~((1 << (len - plen)) - 1);
    if ( (*p1 & mask) != (*p2 & mask) ) {
        return false;
    }
    return true;
}

/**
 * @name  plen2level
 *
 * @brief Finds the trie level of the given plefix length
 *
 * @param[in] pt   Pointer to the routing table
 * @param[in] plen prefix length
 *
 * @retval int The trie level of `plen'
 */
static inline int
plen2level (rtTable* pt, int plen)
{
    int l;


    l = 0;
    for (;;) {
        plen -= pt->psi[l].sl;
        if ( plen <= 0 ) break;
        ++l;
    }
    return l;
}


/**
 * @name  fringeIndex
 *
 * @brief Returns the fringe index.
 *          It takes `nBits' bits from bit `**p + *offset'
 *          and stores them to `c'.
 *        Side effect:
 *          `*p' and `*offset' are advanced after the process.
 *        Requirement:
 *          `nBits' must be <= 24, `*offset' must be < 8.
 *
 * @param[in]    nBits  Stride length to calculate the fringe index
 * @param[inout] p      Pointer to the pointer of the destination address
 * @param[inout] offset Pointer to the bit offset from '*p'
 *
 * @retval u32 The fringe index
 */
static inline u32
fringeIndex (u8** p, u32* offset, int nBits)
{
    u32 b, c;

    c = *offset;
    assert((c < 8) && (nBits <= 24));

    b = c + nBits;
    if ( b <= 8 ) {
        c = (**p >> (8 - b)) & ((1 << nBits) - 1);
    } else if ( b <= 16 ) {
        b -= 8;                 /* offset of byte *((*p)+1) */
        c  = ((*((*p)++) & ((1 << (8 - c)) - 1)) << b);
        c |= (**p >> (8 - b));
    } else if ( b <= 24 ) {
        b -= 16;                /* offset of byte *((*p)+2) */
        c  = ((*((*p)++) & ((1 << (8 - c)) - 1)) << (8 + b));
        c |= ((*((*p)++)) << b);
        c |= (**p >> (8 - b));
    } else {
        b -= 24;                    /* offset of byte *((*p)+3) */
        c  = ((*((*p)++) & ((1 << (8 - c)) - 1)) << (16 + b));
        c |= ((*((*p)++)) << (8 + b));
        c |= ((*((*p)++)) << b);
        c |= (**p >> (8 - b));
    }
    if ( b == 8 ) {
        b = 0;
        (*p)++;
    }
    *offset = b;
    return c + (1 << nBits);
}


/**
 * @name   baseIndex
 *
 * @brief Returns the base index
 *
 * @param[in] pTbl  Pointer to the routing table
 * @param[in] p     Poiner to the destination address of the route
 * @param[in] oplen Prefix length of the route
 *
 * @retval int The base index
 */
static inline int
baseIndex (rtTable* pTbl, u8* p, int plen)
{
    register int len  = 0;
    register int sl;            /* stride length at level `i' */
    register int i;
    u32 st;                     /* stride */

    for ( i = 0;; ++i ) {
        assert(i < pTbl->nLevels);

        sl = pTbl->psi[i].sl;
        if ( plen <= len + sl ) break;
        len += sl;
    }
    p += (len >> 3);         /* sl/8 */

    plen = plen - len;          /* prefix length in stride */
    i = len & 7;                /* i = bit offset in first byte */
    len = i + sl;               /* len = offset + stride length */
    if ( len <= 8 ) {
        st  = (*p >> (8 - len)) & ((1 << sl) - 1);
    } else if ( len <= 16 ) {
        len -= 8;               /* offset in second byte */
        st = ((p[0] & ((1 << (8 - i)) - 1)) << len) | (p[1] >> (8 - len));
    } else if ( len <= 24 ) {
        len -= 16;              /* offset in third byte */
        st = ((p[0] & ((1 << (8 - i)) - 1)) << (8 + len)) |
            (p[1] << len) | (p[2] >> (8 - len));
    } else {
        assert(len <= 32);

        len -= 24;
        st = ((p[0] & ((1 << (8 - i)) - 1)) << (16 + len)) |
            (p[1] << (8 + len)) | (p[2] << len) | (p[3] >> (8 - len));
    }

    return (st >> (sl - plen)) + (1 << plen);
}


/**
 * @name   rtArtAllot
 *
 * @brief  1. Starting from index `k' of subtable (trie node) `t',
 *            visits all the child indices of `k'.
 *         2. Replaces `r' with `s' if the visited indices has `r'
 *         3. Stop visiting when the visited index does not have `r'.
 *            This is because the rest of child indices are supposed to
 *            have more specific route.
 *         Note:
 *         This function was originally written by Donald Knuth in CWEB.
 *         The complicated `goto's are the result of expansion of
 *         two recursions.
 *
 * @param[in] t           Pointer to a subtable (trie node)
 * @param[in] k           Index to start process.
 *                        `k' must be smaller than `threshold'
 * @param[in] r           Route pointer to be replaced with 's'
 * @param[in] s           Route pointer to replace 'r'
 * @param[in] threshold   The first fringe index of 't'
 * @param[in] fringeCheck False if `t' is the deepest level. Otherwise true.
 *
 * @retval int The base index
 */
static inline void
rtArtAllot (subtable t, int k, routeEnt *r,
            routeEnt *s, int threshold, bool fringeCheck)
{
    register int j = k;

    assert( k < threshold );

startChange:
    j <<= 1;
    if ( j < threshold ) goto nonFringe;


    /*  change fringe nodes
     */
    while (1) {
        if ( fringeCheck && isSubtable(t[j]) ) {
            if ( subtablePtr(t[j]).down[1].ent == r ) {
                subtablePtr(t[j]).down[1].ent = s;
            }
        } else if ( t[j].ent == r ) {
            t[j].ent = s;
        }
        if ( j & 1 ) goto moveUp;
        j++;
    }

nonFringe:
    if (t[j].ent == r) goto startChange;
moveOn:
    if (j & 1) goto moveUp;
    j++;
    goto nonFringe;
moveUp:
    j >>= 1;
    t[j].ent = s;               /* change non-fringe node */
    if (j != k) goto moveOn;
}


#endif /* __ipArt_h__ */
