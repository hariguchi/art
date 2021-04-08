/** @file ipArtPathComp.c
    @brif Allotment Routing Table with path-compressed trie


   ART: Allotment Routing Table

   Copyright (c) 2001-2016
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


   Basic Algorithm
   Assume 3-bit address space. Apply the following mapping function

     baseIndex(addr, plen) { return (addr >> (3 - plen)) + (1 << plen); }

   Here, `addr' is an address (0-7), `plen' is a prefix length (0-3).
   baseIndex() maps all the possible prefixes into the folloing heap.

                               1
                             (0/0)
                            /     \
                          /         \
                        /             \
                      /                 \
                    /                     \
                   2                       3
                 (0/1)                   (4/1)
                /     \                 /     \
               /       \               /       \
              /         \             /         \
             4           5           6           7
           (0/2)       (2/2)       (4/2)       (6/2)
          /    \      /    \      /    \      /    \
         8     9     10    11    12    13    14    15
       (0/3) (1/3) (2/3) (3/3) (4/3) (5/3) (6/3) (7/3)

   Let us call a node in the deepest level of heap (node 8 to 15)
   `fringe node'. Let us call the index of the node corresponding
   to a prefix `base index'. Let `s' be a pointer of a route entry
   to be inserted. Let `X' be a heap. The basic insertion algorithm
   is as follows. First, ART obtains a route pointer (say `r') at
   the base index of `*s' (i = baseIndex(s->addr, s->plen), r = X[i]).
   Second, ART calls a function called `allot()'. `allot()' takes 3
   parameters: `i' (base index), `r' (route to be replaced), and
   `s' (route that replaces `r').
   `allot()':
     1. replaces `r' with `s' (X[i] = s).
     2. checks if `X[i]' is a fringe node or not. If `X[i]' is a fringe node,
        `allot()' returns. Otherwise it continues the process
        (if ( i >= 8 ) return;).
     3. checks whether the value of the child node on the left hand side
        (X[i<<1]) is equal to `r'. If so, it is necessary to replace the
        childe nodes starting at `X[i<<1]'. Hence it calls itself recursively
        ( if ( X[i<<1] == r ) allot(i<<1, r, s);)
     4. checks whetehr the value of the child node on the right hand side
        (X[(i<<1)+1]) is equal to `r'. If so, it is necessary to replace the
        childe nodes starting at `X[(i<<1)+1]'. Hence it calls itself
        recursively ( if ( X[(i<<1)+1] == r ) allot((i<<1)+1, r, s);)

   In other words, `allot()' replaces all the child nodes that point to
   the same route that `r' points to with `s'.

   Search is very easy. Let `addr' be a search key.
   Just return `X[addr + (1<<3)]' since the fringe nodes have a pointer
   to the route. The reason `allot()' allots route pointers not only
   to the fringe nodes but also non-fringe nodes is to reduce the
   deletion cost.

   Assume the route at base index `i' is removed. We have to find route
   to replace all the child nodes of `X[i]' that points to the route at
   `X[i]'. It is very easy in ART. The route is obtained as `X[i>>1]'.
   That is why the deletion is done as `allot(i, X[i], X[i>>1])'.
*/


#include "ipArt.h"

#define NullEnt ((tableEntry){0})


/**
 * @name  strideLen
 *
 * @brief Gets the stride length (in bits) of the given trie level
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] level Level of subtable (trie node)
 *
 * @retval size_t The stride length (in bits) of level `level'
 */
static inline size_t
strideLen (rtTable* pt, int level)
{
    return pt->psi[level].tl;
}


/**
 * @name  addrCpy
 *
 * @brief Copies the given number of bits.
 *
 * @param[in] dst   Pointer to the destination bit string
 * @param[in] src   Pointer to the source bit string
 * @param[in] nBits The number of bits to be copied
 */
static inline void
addrCpy (void* dst, void* src, u32 nBits)
{
    u32 nBytes;
    u32 len;
    u32 n;


    nBytes = bits2bytes(nBits);
    len = nBytes << 3;
    n = (len > nBits) ? (nBytes - 1) : nBytes;
    memcpy(dst, src, n);
    if (n < nBytes) {
        ((u8*)dst)[n] = ((u8*)src)[n] & (~((1 << (len - nBits)) - 1));
    }
}


/**
 * @name  findSubtable
 *
 * @brief Finds the first subtable (trie node) pointer in the
 *        given subtable
 *
 * @param[in] pt Pointer to the routing table
 * @param[in] t  Pointer to the subtable (trie node) to be searched
 *
 * @retval subtable Pointer to the first subtable in `t'
 */
static inline subtable
findSubtable (rtTable* pt, subtable t)
{
    int l = t[-1].level;
    register int i   = 1 << pt->psi[l].sl;
    register int max = i << 1;

    for ( ; i < max; ++i ) {
        if ( isSubtable(t[i]) ) {
            return (t + i);
        }
    }
    return NULL;
}

#ifdef DEBUG_FREE_HEAP
/**
 * @name  checkSubtable
 *
 * @brief Checks the consistency of a subtable (trie node.)
 *        This is a function for debugging.
 *
 * @param[in] pt      Pointer to the routing table
 * @param[in] t       Pointer to the subtable (trie node) to be checked
 * @param[inout] flag Pointer to the flag indicating as follows:
 *                     *flag == true:  `t[0].nRoutes' is already decremented
 *                                     by the caller. This function sets
 *                                     `*flag' to false.
 *                     *flag == false: `t[0].nRoutes' is NOT decremented
 *                                     by the caller.
 */
static void
checkSubtable (rtTable* pt, subtable t, bool* flag)
{
    register int i, j, max, plen;
    routeEnt* pEnt;
    int l, nRoutes, nSubtables;

    l          = t[-1].level;
    nRoutes    = 0;
    nSubtables = 0;
    plen = (l == 0) ? 0 : (pt->psi[l-1].tl + 1);
    max = 1 << pt->psi[l].sl;

    assert(l < pt->nLevels);

    /*
     * Iterate non-fringe indices.
     * There should not be any subtable pointers.
     * t[1].ent will be taken care of when
     * fringe indices are visited.
     */
    j = 4;
    for ( i = 2; i < max; ++i ) {
        assert(!isSubtable(t[i]));

        if ( i == j ) {
            /*
             * Reached the beginning of the next prefix lengh.
             */
            j <<= 1;
            ++plen;
        } else {
            assert(i < j);
        }
        pEnt = t[i].ent;
        if ( pEnt && (pEnt->plen == plen) ) {
            ++nRoutes;      /* increment the number of routes */
        }
    }
    plen = pt->psi[l].tl;
    max <<= 1;
    for ( ; i < max; ++i ) {
        if ( isSubtable(t[i]) ) {
            subtable pst = subtablePtr(t[i]).down;
            pEnt = pst[1].ent;
            ++nSubtables;
            if ( pEnt && (pEnt->plen == plen) ) {
                ++nRoutes;
            }
        } else if ( t[i].ent ) {
            pEnt = t[i].ent;
            if ( pEnt->plen == plen ) {
                ++nRoutes;
            }
        }
    }
    if ( *flag ) {
        /*
         * Need to decrement `nRoutes' because
         * `t[0].nRoutes' is decremented by the caller.
         * Unset `*flag' so that the caller does not have
         * to check the value of `*flag' and update it.
         */
        --nRoutes;
        *flag = false;
    }
    assert(nRoutes == t[0].nRoutes);
    assert(nSubtables == t[0].nSubtables);
}
#define CHECKSUBTABLE(_pt,_t,_f) checkSubtable((_pt), (_t), (_f))
#else
#define CHECKSUBTABLE(_pt,_t,_f)
#endif /* DEBUG_FREE_HEAP */


/**
 * @name  rtArtPcNewSubTable
 *
 * @brief Allocates a new subtable (trie node) of the given level.
 *
 * @param[in] p     Pointer to the routing table
 * @param[in] level
 * @param[in] base  Subtable (trie node) default route. Assume a
 *                  newly allocated subtable (pst) is connected to
 *                  the index (i) of parent subtable (pn). `base'
 *                  must be pn[i].ent if pn[i] is a route entry.
 * @param[in] pa    Pointer to the prefix of the route to be inserted
 */
static inline subtable
rtArtPcNewSubTable (rtTable* p, int level, tableEntry base, u8* pa)
{
    register subtable t;
    register int a;
    size_t len;


    assert(p && pa && level >= 0 && level < p->nLevels);

    /*
     * 1 << (p->psl[level]+1: # of entries in heap (subtable)
     * 1:                     t[-1] that holds heap level
     * bytes2nPtrs(p->len):   address to calc. parent heaps' fringe indices
     */
    a = -(p->off);              /* a = bytes2nPtrs(p->len) + 1; */
    t = (subtable)calloc((1 << (p->psi[level].sl+1))+a, sizeof(tableEntry));
    if (!t) return t;

    /*
     * save the prefix representing this subtable
     * which is up to level `level' - 1
     */
    if ( level > 0 ) {
        len = strideLen(p, level - 1);
        addrCpy(t, pa, len);    /* address cache */
    }
    t = t + a;

    assert(((size_t)t & (sizeof(size_t) - 1)) == 0);

    t[-1].level = level; /* save level */
    t[1] = base;         /* trie node default route */

    return t;
}


/**
 * @name  getNodeDefAddr
 *
 * @brief Gets the subtable (trie node) default address
 *
 * @param[in] p Pointer to the routing table
 * @param[in] t Pointer to the beginning of subtable (trie node)
 *
 * @retval u8* Pointer to the subtable default address
 */
static inline u8*
getNodeDefAddr (rtTable *p, subtable t)
{
    return (u8*)(t + p->off);
}


/**
 * @name  rtArtPcFreeSubtable
 *
 * @brief Frees the memory allocated for a subtable (trie node)
 *
 * @param[in] pt Pointer to the routing table
 * @param[in] t  Pointer to the subtable to be freed
 *
 * @retval tableEntry Subtable default route in the freed subtable.
 *                    This must be restored in the parent subtable.
 */
static inline tableEntry
rtArtPcFreeSubtable (rtTable* pt, subtable t)
{
    register tableEntry base;   /* heap default route */


    assert(pt && t);

    base = t[1];
    free(t + pt->off);
    ++pt->nSubtablesFreed;

    return base;
}


/**
 * @name  firstDiffLevel
 *
 * @brief Finds the trie level the first difference occurred
 *
 * @param[in] p     Pointer to the routing table
 * @param[in] index p1[index] and p2[index] are the first different bytes.
 * @param[in] p1    Pointer to an address
 * @param[in] p2    Pointer to an address
 *
 * @retval integer The trie level that the first difference occurred.
 */
static inline int
firstDiffLevel (rtTable* p, int index, u8* p1, u8* p2)
{
    u32 offset;
    u8  mask;
    int nBits;
    int l;

    nBits = index << 3;
    for (l = 0; l < p->nLevels; ++l) {
        if ( p->psi[l].tl > nBits ) {
            offset = p->psi[l].tl - nBits;

            while ( offset > 8 ) {
                if ( p1[index] != p2[index] ) {
                    return l;
                }
                offset >>= 1;
                ++index;
            }
            mask = ~((1 << (8 - offset)) - 1);
            if ( (p1[index] & mask) == (p2[index] & mask) ) {
                return l + 1;
            }
            return l;
        }
    }
    panic(("firstDiffLevel(): should not happen"));
}


/**
 * @name  setStartBitPos
 *
 * @brief Sets the pointer and bit offset (from bit 7) of the given
 *        trie level for fringe index calculation.
 *
 * @param[in] pt   Pointer to the routing table
 * @param[in] l    Trie level of the address pointed to by `*p'
 * @param[inout] p `*p' must point to the beginning of the address
 *                 as an input. This function updates `*p' pointing to
 *                 the beginning byte of trie level `l'
 * @param[out]     Pointer to the bit offset (from bit 7) of `**p'.
 */
static inline void
setStartBitPos (rtTable* pt, u8** p, u32* offset, int l)
{
    if ( l == 0 ) {
        *offset = 0;
    } else {
        --l;
        (*p)    += (pt->psi[l].tl >> 3);
        *offset  = pt->psi[l].tl & 7;
    }
}


/**
 * @name   rtArtInsert
 *
 * @brief  Inserts route `s' into subtable (trie node) `t'
 *         of the routing table `pt'
 *
 * @param[in] pt          Pointer to the routing table
 * @param[in] t           Pointer to a subtable (trie node)
 * @param[in] k           Index to start process.
 *                        `k' must be smaller than `threshold'
 * @param[in] s           Route pointer to be inserted
 * @param[in] threshold   The first fringe index of 't'
 * @param[in] fringeCheck False if `t' is the deepest level. Otherwise true.
 *
 * @retval routeEnt* 1. `s' if route pointer `s' is successfully inserted.
 *                   2. !`s' if there is an existing route.
 *                      `s' must be freed in this case.
 */
static inline routeEnt*
rtArtInsert (rtTable* pt, subtable t, int k,
             int threshold, bool fringeCheck, routeEnt* s)
{
    register tableEntry z = t[k];
    register routeEnt *r;

    assert(((t != NULL) && (s != NULL)));

    r = (fringeCheck && isSubtable(z)) ? subtablePtr(z).down[1].ent : z.ent;
    if ( r && (r->plen == s->plen) &&
        (cmpAddr(r->dest, s->dest, s->plen) == true) ) {
        return r;
    }

    t[0].nRoutes++;
    if ( k < threshold ) {
        rtArtAllot(t, k, r, s, threshold, fringeCheck);
    } else if ( fringeCheck && isSubtable(z) ) {
        subtablePtr(z).down[1].ent = s;
    } else {
        t[k].ent = s;
    }
    pt->nRoutes++;
    return s;
}


/**
 * @name  rtArtPcFindMatch
 *
 * @brief API Function.
 *        (registered as `pt->findMatch()' in `rtArtPcInit()').
 *        Performs the longest prefix match.
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] pDest Pointer to the IP address to be searched for
 *
 * @retval routeEnt* Pointer to the found route entry (success)
 * @retval NULL      Failed to find a matching route entry
 */
static routeEnt *
rtArtPcFindMatch (rtTable* pt, u8* pDest)
{
    register tableEntry   ent;
    register tableEntry*  pst;
    register routeEnt**   pDefRoute;
    register int l;
    u8* pAddr;
    u32 offset;
    int ml;                     /* max level */


    pst = pt->root;
    ml  = pt->nLevels - 1;
    pDefRoute = pt->pDef;
    for ( l = pst[-1].level; l <= ml; l = pst[-1].level ) {
        pAddr = pDest;
        setStartBitPos(pt, &pAddr, &offset, l);
        ent = pst[fringeIndex(&pAddr, &offset, pt->psi[l].sl)];
        if ( !ent.ent ) {
            break;
        }
        if ( !isSubtable(ent) ) {
            if ( cmpAddr(pDest, ent.ent->dest, ent.ent->plen) ) {
                return ent.ent;
            }
            break;
        }

        assert(l < ml);

        /*
         * 1. Go to the next subtable (trie node)
         * 2. Remember the trie-node default route if exists.
         */
        ent = subtablePtr(ent);
        pst = ent.down;
        if ( pst[1].ent ) {
            *pDefRoute++ = pst[1].ent;
        }
    }

    /*
     * No match
     */
    while ( --pDefRoute >= pt->pDef ) {
        assert(*pDefRoute);

        if ( cmpAddr(pDest, (*pDefRoute)->dest, (*pDefRoute)->plen) ) {
            return *pDefRoute;
        }
    }
    return pt->root[1].ent;     /* default route */
}


/**
 * @name  rtArtPcFindExactMatch
 *
 * @brief API Function.
 *        (registered as `pt->findExactMatch()' in `rtArtPcInit()').
 *        Performs the exact match (address + prefix length.)
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] pDest Pointer to the IP address to be searched for
 * @param[in] plen  prefix length of `pDest'
 *
 * @retval routeEnt* Pointer to the found route entry (success)
 * @retval NULL      Failed to find a matching route entry
 */
static routeEnt *
rtArtPcFindExactMatch (rtTable* pt, u8* pDest, int plen)
{
    register tableEntry  ent = {0};
    register tableEntry* pst;
    register int index = 0;
    register int l;
    register int ml;            /* max level */
    u8* pAddr;
    u32 offset;


    pst = pt->root;
    ml  = plen2level(pt, plen);
    for ( l = pst[-1].level; l <= ml; l = pst[-1].level ) {
        pAddr = pDest;
        setStartBitPos(pt, &pAddr, &offset, l);
        index = fringeIndex(&pAddr, &offset, pt->psi[l].sl);
        ent   = pst[index];
        if ( !ent.ent ) {
            /*
             * Neither route entry nor subtable pointer.
             * Return the default route pointer.
             */
            return pt->root[1].ent;    /* default route */
        }
        if ( !isSubtable(ent) ) {
            goto AddrComp;
        }
        if ( l == ml ) {
            ent.ent = subtablePtr(ent).down[1].ent;
            break;
        }

        /*
         * Go to the next subtable (trie node)
         */
        ent = subtablePtr(ent);
        pst = ent.down;
    }
    assert(1);                  /* should not happen */

AddrComp:
    while ( index > 0 ) {
        if ( !ent.ent ) {
            break;              /* no matching route */
        }
        if ( (ent.ent->plen == plen) &&
             cmpAddr(pDest, ent.ent->dest, ent.ent->plen) ) {
            return ent.ent;
        }
        index >>= 1;
        ent = pst[index];
    }

    return pt->root[1].ent;      /* default route */
}


/**
 * @name  insertNewSubtable
 *
 * @brief Inserts a new subtable (trie node)
 *
 *
 * @param[in] p     Pointer to the routing table
 * @param[in] pEnt  Pointer to the route entry to be inserted
 * @param[in] pst   Pointer to the beginning of subtable (trie node)
 * @param[in] pst2   Pointer to the union of
 *                  subtable pointer and route entry pointer
 * @param[in] level Trie level of the subtable to be inserted
 * @param[in] index base index of `*pEnt'
 *
 * @retval routeEnt* Succeeded to insert `pEnt' to the routing table
 * @retval NULL      Failed to insert new subtable(s)
 */
routeEnt*
insertNewSubtable (rtTable* p, routeEnt* pEnt,
                   subtable pst, tableEntry* pst2, int level, int index)
{
    subtable nst;
    subtable nst2;
    tableEntry ent;
    tableEntry e;
    u8* pDest;
    u32 offset;
    int i;
    int l;
    int sl;
    bool flag;


    assert(p && pEnt);

    l     = pEnt->level;
    e.ent = NULL;
    ent   = *pst2;
    if ( isSubtable(ent) ) {
        assert(level > pst[-1].level);

        ent = subtablePtr(ent);

        assert(level < (ent.down)[-1].level);

        nst2 = rtArtPcNewSubTable(p, level, e, pEnt->dest);
        if ( !nst2 ) {
            return NULL;
        }
        if ( l == level ) {
            /*
             * One new subtable (trie node) is enough since
             * `pEnt' will be allotted in `nst2' (same level)
             */
            nst = nst2;
        } else {
            assert(level < l);

            /*
             * `pEnt' is allotted in `nst'
             */
            nst = rtArtPcNewSubTable(p, l, e, pEnt->dest);
            if ( !nst ) {
                free(nst2);
                return NULL;
            }
            /*
             * Connect `nst' to `nst2'
             */
            pDest = pEnt->dest;
            setStartBitPos(p, &pDest, &offset, level);
            i = fringeIndex(&pDest, &offset, p->psi[level].sl);
            nst2[i].down = makeSubtable(nst);
            nst2[0].nSubtables++;
        }
        /*
         * 1. Connect `nst2' to the existing subtable (trie node)
         * 2. Connect `ent.down' to `nst2'
         */
        pDest = getNodeDefAddr(p, ent.down);
        setStartBitPos(p, &pDest, &offset, level);
        i = fringeIndex(&pDest, &offset, p->psi[level].sl);
        nst2[1] = (ent.down)[1];
        (ent.down)[1].ent = NULL;
        pst[pst2 - pst].down = makeSubtable(nst2);
        nst2[i].down = makeSubtable(ent.down);
        nst2[0].nSubtables++;
    } else {
        assert(level == l);

        nst = rtArtPcNewSubTable(p, level, ent, pEnt->dest);
        if ( !nst ) {
            return NULL;
        }
        pst[pst2 - pst].down = makeSubtable(nst);
        pst[0].nSubtables++;
    }
    flag = (l >= (p->nLevels - 1)) ? false : true;
    sl = p->psi[l].sl;
    return rtArtInsert(p, nst, index, 1 << sl, flag, pEnt);
}


/**
 * @name  rtArtPcInsertRoute
 *
 * @brief API function.
 *        (registered as `pt->insert()' in `rtArtPcInit()').
 *        Adds a route represented by `pEnt' to the routing table `pt'
 *
 * @param[in] pt   Pointer to the routing table
 * @param[in] pEnt Pointer to the route added to `pt'.
 *                 `pEnt' must NOT point to a local variable.
 *
 * @retval routeEnt* `pEnt' is successfully inserted.
 *         pEnt      There is an existing route that has the same
 *                   IP prefix (address and prefix length).
 *                   `pEnt' must be freed in this case.
 *
 */
static routeEnt*
rtArtPcInsertRoute (rtTable* pt, routeEnt* pEnt)
{
    register int l, sl, endBit;
    register subtable   pst;
    register tableEntry ent;
    subtable pst2;
    u8*  pDest;
    u8*  pDef;               /* default address in a node */
    int  i;                  /* first diff occured at the i-th byte */
    int  nl;                 /* new level */
    int  index;
    u32  offset;
    bool flag;


    assert((pt != NULL) && (pEnt != NULL));

    pEnt->level = plen2level(pt, pEnt->plen);

    /*
     * Handle default route
     */
    if ( pEnt->plen == 0 ) {
        if ( pt->root[1].ent ) {
            return pt->root[1].ent;
        }
        pt->root[1].ent = pEnt;
        pt->nRoutes++;
        return pEnt;
    }

    index = baseIndex(pt, pEnt->dest, pEnt->plen);
    pst   = pt->root;           /* ptr to subtable */
    l     = 0;                  /* level */
    while ( l < pt->nLevels ) {
        sl    = pt->psi[l].sl;
        pst2  = pst;
        pDest = pEnt->dest;     /* ptr to dest. IP address */
        setStartBitPos(pt, &pDest, &offset, l);
        pst += fringeIndex(&pDest, &offset, sl);
        ent  = *pst;
        if ( (pEnt->level > 0) && isSubtable(ent) ) {
            ent  = subtablePtr(ent);
            l    = (ent.down)[-1].level; /* level of subtable */
            pDef = getNodeDefAddr(pt, ent.down);

            assert(l > 0);

            /*
             * bitStrCmp(): comparison finishes
             * at the `i'-th byte from 'pDef'
             */
            if ( l < pEnt->level ) {
                endBit = pt->psi[l - 1].tl - 1;
            } else {
                endBit = pt->psi[pEnt->level - 1].tl - 1;
            }
            if ( !bitStrCmp(pDef, pEnt->dest, 0, endBit, &i) ) {
                if ( pEnt->level > l ) {
                    pst = ent.down;
                    continue;
                } else if ( pEnt->level < l ) {
                    /*
                     * Need to insert a new subtable
                     */
                    nl = pEnt->level;
                } else {
                    /*
                     * Next level is my level. Try to insert a route.
                     */
                    assert(pEnt->level == l);

                    pst  = ent.down;
                    sl   = pt->psi[l].sl;
                    flag = (l >= (pt->nLevels - 1)) ? false : true;
                    return rtArtInsert(pt, pst, index, 1 << sl, flag, pEnt);
                }
            } else {
                nl = firstDiffLevel(pt, i, pDef, pEnt->dest);
            }
            if ( nl < l ) {
                /*
                 * Add a new subtable (whose level is always < l)
                 */
                return insertNewSubtable(pt, pEnt, pst2, pst, nl, index);
            }
            assert(nl == l);

            pst = ent.down;
            if ( pst[1].ent && (pst[1].ent->plen == pEnt->plen) ) {
                return pst[1].ent;
            }
            continue;
        } else {
            /*
             * `ent' is either a route pointer or NULL
             */
            nl = pEnt->level;
        }
        if ( nl == l ) {
            flag = (l >= (pt->nLevels - 1)) ? false : true;
            return rtArtInsert(pt, pst2, index, 1 << sl, flag, pEnt);
        }
        /*
         * Add a new subtable (whose level is always > l)
         */
        assert(nl > l);
        return insertNewSubtable(pt, pEnt, pst2, pst, nl, index);
    }
    panic(("rtArtInsertRoute: should not happen"));
    return NULL;
}


/**
 * @name   rtArtDelete
 *
 * @brief  Deletes the route represented by an IP prefix (address
 *         and its prefix length) from the routing table
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] pPcSt Pointer to the bottom of an array of path-compressed
 *                  trie information in order to reach level `l'
                    subtable (level `l' trie node)
 * @param[in] pEnt  NULL if there is no subtable default route in the
 *                  parent subtable (i.e., pPcSt->pst[1] == NULL)
 *                  Otherwise `pEnt' must be equal to `pPcSt->pst[1]'.
 * @param[in] l     The subtable level (trie level) of the route
 *                  to be deleted.
 * @param[in] pDest Pointer to the IP address to be deleted from `pt'
 * @param[in] plen  Prefix length associated with `pDest'
 */
static bool
rtArtPcDelete (rtTable* pt, pcSubtbls* pPcSt,
               routeEnt* pEnt, int l, u8* pDest, int plen)
{
    register subtable   t;
    register tableEntry z;
    register routeEnt*  r;      /* route to be deleted */
    register routeEnt*  s;      /* route to replace 'r' */
    routeEnt *save;
    bool fringeCheck;
    int  k;
    int  threshold;
#ifdef DEBUG_FREE_HEAP
    bool flag = true;           /* for debugging */
#endif /* DEBUG_FREE_HEAP */


    assert(l == plen2level(pt, plen));
    assert(pPcSt && pPcSt->pst && (pPcSt >= pt->pPcSt));

    t = pPcSt->pst;
    k = baseIndex(pt, pDest, plen);
    threshold = 1 << pt->psi[l].sl;
    fringeCheck = (l >= (pt->nLevels - 1)) ? false : true;
    z = t[k];
    r = pEnt ? pEnt : z.ent;

#ifdef DEBUG_FREE_HEAP
    if ( isSubtable(z) ) {
        assert(pEnt == subtablePtr(z).down[1].ent);
    }
#endif /* DEBUG_FREE_HEAP */

    if ( (!r) || (r->plen != plen) ||
        (cmpAddr(r->dest, pDest, plen) == false) ) {
        return false;
    }

    --pt->nRoutes;
    --t[0].nRoutes;
    save = r;
    s = ((k >> 1) > 1) ? t[k >> 1].ent : NULL;
    while ( pPcSt > pt->pPcSt ) {
        register subtable pst;

        CHECKSUBTABLE(pt, t, &flag);

        if ( t[0].nRoutes > 0 ) {
            break;
        }
        if ( t[0].nSubtables > 1 ) {
            break;
        }

        if ( t[0].nSubtables == 1 ) {
            /*
             * Connect down-level subtable to up-level subtable
             */
            pst = findSubtable(pt, t);

            assert(isSubtable(*pst));

            pst = subtablePtr(*pst).down;

#ifdef DEBUG_FREE_HEAP
            if ( pst[1].ent ) {
                /*
                 * nRoutes == 0.
                 * pst[1].ent must be the route to be deleted if exists.
                 */
                assert(save == pst[1].ent);
            }
#endif /* DEBUG_FREE_HEAP */

            /*
             * Keep subtable default route in the next level
             * because `t' will be freed.
             */
            pst[1].ent = t[1].ent;

            --pPcSt;
            pPcSt->pst[pPcSt->idx].down = makeSubtable(pst);
        } else {
            assert(t[0].nSubtables == 0);
            assert(t[0].nRoutes == 0);

            --pPcSt;
            pPcSt->pst[pPcSt->idx].ent = t[1].ent;
            --pPcSt->pst[0].nSubtables;
        }

        r = rtArtPcFreeSubtable(pt, t).ent; /* r = t[1].ent */
        t = pPcSt->pst;                     /* update t */
    }

    if ( r == save ) {
        /*
         * Update subtable `t'
         */
        if ( k < threshold ) {
            rtArtAllot(t, k, r, s, threshold, fringeCheck);
        } else if ( fringeCheck && isSubtable(z) ) {
            subtablePtr(z).down[1].ent = s;
        } else {
            t[k].ent = s;
        }
    }

    free(save);
    return true;
}


/**
 * @name  rtArtPcDeleteRoute
 *
 * @brief API function.
 *        (registered as `pt->delete()' in `rtArtPcInit()').
 *        Deletes a route represented by an IP prefix
 *        (address and its prefix length) from the routing table.
 *        The matched route entry is freed in this function.
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] pDest Pointer to the IP address to be deleted from `pt'
 * @param[in] plen  Prefix length associated with `pDest'
 *
 * @retval ture  If the matching route is deleted from `pt'.
 *               The route entry is freed in this function.
 * @retval false If there is no matching route in `pt'.
 */
bool
rtArtPcDeleteRoute (rtTable* pt, u8* pDest, int plen)
{
    register int      l;        /* level */
    register subtable pst;
    register subtable pst2;
    routeEnt*  pEnt;
    pcSubtbls* pPcSt;
    u8*        pAddr;
    u32        offset;
    int        ml;


    assert(pt && pDest);

    /*
     * Handle default route
     */
    if (plen == 0) {
        if ( pt->root[1].ent == NULL ) return false;
        rtArtFreeRoute(pt, pt->root[1].ent);
        pt->root[1].ent = NULL;
        --pt->nRoutes;
        return true;
    }

    pst   = pt->root;
    pPcSt = pt->pPcSt;
    ml    = plen2level(pt, plen);
    pEnt  = NULL;

    assert(ml < pt->nLevels);

    for ( l = pst[-1].level; l <= ml; l = pst[-1].level ) {
        pAddr = pDest;
        setStartBitPos(pt, &pAddr, &offset, l);
        pPcSt->idx = fringeIndex(&pAddr, &offset, pt->psi[l].sl);
        pPcSt->pst = pst;
        pst += pPcSt->idx;
        if ( isSubtable(*pst) ) {
            pst2 = subtablePtr(*pst).down;
            if ( pst2[1].ent ) {
                if ( pst2[1].ent->plen == plen ) {
                    pEnt = pst2[1].ent;
                }
                if ( l == ml ) {
                    goto exit;
                }
            }
            pst = pst2;
            ++pPcSt;
        } else {
            if ( l < ml ) {
                return false;
            }
            goto exit;
        }
    }
    return false;

exit:
    return rtArtPcDelete(pt, pPcSt, pEnt, l, pDest, plen);
}


/**
 * @name  rtArtPcDestroy
 *
 * @brief API Function.
 *        (registered as `pt->deleteTable()').
 *        Delete all the route entries in the routing table, then
 *        Free the routing table itself.
 * @param[in,out] p Pointer to the pointer to `rtTable'. `*p' is
 *                  set to NULL at the end of this function.
 */
static void
rtArtPcDestroy (rtTable** p)
{
    rtTable* pt = *p;

    pt->flush(pt);
    free(pt->pPcSt);
    free(pt->root + pt->off);
    free(pt->pEnt);
    free(pt->psi);
    free(pt);
    *p = NULL;
}


/**
 * @name  rtArtPcInit
 *
 * @brief Initializes a path-compressed routing table
 *
 * @param[in] pt Pointer to the routing table
 *
 * @retval rtTable* Pointer to the initialized routing table
 * @retval NULL     Failed to initialize `pt' (which is freed
 *                  in this function.)
 */
rtTable*
rtArtPcInit (rtTable *pt)
{
    tableEntry base;
    u8* defAddr;

    assert(pt);
    assert(pt->psi);

    defAddr = calloc((pt->len), 1);
    if ( !defAddr ) {
        goto  slFree;
    }
    pt->pPcSt = calloc(pt->nLevels, sizeof(pcSubtbls));
    if ( !pt->pPcSt ) {
        goto defAddrFree;
    }
    pt->pDef = calloc(pt->nLevels, sizeof(pt->pDef));
    if ( !pt->pDef ) {
        goto pcStFree;
    }

    base.ent = NULL;
    pt->root = rtArtPcNewSubTable(pt, 0, base, defAddr);

    pt->insert         = rtArtPcInsertRoute;
    pt->delete         = rtArtPcDeleteRoute;
    pt->deleteTable    = rtArtPcDestroy;
    pt->flush          = rtArtFlushRoutes;
    pt->findMatch      = rtArtPcFindMatch;
    pt->findExactMatch = rtArtPcFindExactMatch;

    free(defAddr);
    return pt;

pcStFree:
    free(pt->pPcSt);
defAddrFree:
    free(defAddr);
slFree:
    free(pt->psi);
    free(pt);
    return NULL;
}
