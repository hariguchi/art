/** @file ipArt.c
    @brif Allotment Routing Table with simple trie


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


/**
 * @name  rtArtNewSubTable
 *
 * @brief Allocates a new subtable (trie node)
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] level level of the subtable to be allocated
 * @param[in] base  Subtable default route stored in index 1
 *
 * @retval subtable Pointer to the allocated subtable (success)
 * @retval NULL     Failed to allocate a subtable
 */
static inline subtable
rtArtNewSubTable (rtTable* pt, int level, tableEntry base)
{
    register subtable t;

    t = (subtable)
        calloc((1 << (pt->psi[level].sl+1)) + 1, sizeof(tableEntry));

    t->level = level; /* save level */
    ++t;              /* make level hidden */
    t[1] = base;      /* subtable default route is stored in index 1 */
    return t;
}


/**
 * @name  rtArtFreeSubTable
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
rtArtFreeSubtable (rtTable* pt, subtable t)
{
    register tableEntry base;   /* heap default route */

    assert(t);

    base = t[1];
    free(t - 1);                /* get the beginning address of buffer */
    ++pt->nSubtablesFreed;

    return base;
}


/**
 * @name  rtArtRootTable
 *
 * @brief Allocates a root subtable (trie node)
 *
 * @param[in] pt Pointer to the routing table
 *
 * @retval subtable Pointer to the allocated root subtable
 * @retval NULL     Failed to allocate a new subtable
 */
static subtable
rtArtRootTable (rtTable* pt)
{
    tableEntry base;


    base.ent = NULL;
    return rtArtNewSubTable (pt, 0, base);
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

    t[0].count++;
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
 * @name   rtArtDelete
 *
 * @brief  Deletes route `s' from subtable (trie node) `t'
 *
 * @param[in] pt          Pointer to the routing table
 * @param[in] t           Pointer to a subtable (trie node)
 * @param[in] k           Index to start process.
 *                        `k' must be smaller than `threshold << 1'
 * @param[in] threshold   The first fringe index of 't'
 * @param[in] fringeCheck False if `t' is the deepest level. Otherwise true.
 * @param[in] pDest       Pointer to the destination IP address to be deleted.
 * @param[in] plen        Prefix length of `pDest'
 * @param[in] l           The level of subtable `t'
 *
 * @retval routeEnt* Pointer to the deleted route.
 * @retval NULL      There was no matching route with
 *                   represented by `pDest' and `plen'
 */
static inline routeEnt *
rtArtDelete (rtTable* pt, subtable t, int k,
             int threshold, bool fringeCheck, u8* pDest, int plen, int l)
{
    register tableEntry z = t[k];
    register routeEnt* r;       /* route to be deleted */
    register routeEnt* s;       /* route to replace 'r' */
    routeEnt *save;


    r = (fringeCheck && isSubtable(z)) ? subtablePtr(z).down[1].ent : z.ent;
    if ( (!r) || (r->plen != plen) ||
        (cmpAddr(r->dest, pDest, plen) == false) ) {
        return NULL;
    }


    pt->nRoutes--;
    save = r;
    s = ((k >> 1) > 1) ? t[k >> 1].ent : NULL;
    while ( l-- >= 0 ) {
        t[0].count--;
        if ( t[0].count > 0 ) break;
        if ( l < 0 ) break;     /* Don't free level 0 table */

#ifdef DEBUG_FREE_HEAP
        {
            int i;
            for ( i = threshold; i < (1 << (pt->psi[l+1].sl+1)); ++i ) {
                assert(!isSubtable(t[i]));
            }
        }
#endif /* DEBUG_FREE_HEAP */

        /*
         * Counter == 0 and level > 0. Free subtable.
         */
        r = rtArtFreeSubtable(pt, t).ent; /* r = t[1].ent */
        pt->pEnt[l]->ent = r;   /* restore heap default route */

        t = pt->pTbl[l];        /* set `t' to parent subtable */
    }
    if ( r != save ) return save; /* subtable(s) are freed */


    /*
     * Update subtable `t'.
     */
    if ( k < threshold ) {
        rtArtAllot(t, k, r, s, threshold, fringeCheck);
    } else if ( fringeCheck && isSubtable(z) ) {
        subtablePtr(z).down[1].ent = s;
    } else {
        t[k].ent = s;
    }
    return save;
}


/**
 * @name   rtArtFindMatch
 *
 * @brief  API function.
 *         (registered as `pt->findMatch()' in `rtArtInit()').
 *         Performs the longest prefix match.
 *
 * @param[in] pt          Pointer to the routing table
 * @param[in] pDest       Pointer to the destination IP address
 *
 * @retval routeEnt* Pointer to the longest prefix matching route.
 * @retval NULL      There was no matching route for `pDest'
 */
static routeEnt *
rtArtFindMatch (rtTable* pt, u8* pDest)
{
    register tableEntry  ent;
    register tableEntry* pst;
    register routeEnt*   pDefRoute;
    register int         l;
    u32 offset;


    pst = pt->root;
    offset = 0;
    pDefRoute = NULL;
    for (l = 0; l < pt->nLevels; ++l ) {
        ent = pst[fringeIndex(&pDest, &offset, pt->psi[l].sl)];
        if ( !ent.ent ) break;
        if ( !isSubtable(ent) ) return ent.ent;
        ent = subtablePtr(ent);
        if ( l >= (pt->nLevels - 1) ) break;
        if ( ent.down[1].ent ) {
            pDefRoute = ent.down[1].ent;
        }
        pst = ent.down;
    }

    /*
     * No match.
     */
    if ( pDefRoute ) {
        return pDefRoute;
    }
    return pt->root[1].ent;
}


/**
 * @name  rtArtFindExactMatch
 *
 * @brief API Function.
 *        (registered as `pt->findExactMatch()' in `rtArtInit()').
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
rtArtFindExactMatch (rtTable* pt, u8* pDest, int plen)
{
    register tableEntry  ent;
    register tableEntry* pst;
    register int index;
    register int l;
    register int ml;            /* max level */
    u8* pAddr;
    u32 offset;


    pst    = pt->root;
    pAddr  = pDest;
    ml     = plen2level(pt, plen);
    offset = 0;
    for ( l = 0; l <= ml; ++l ) {
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
         * 1. Go to the next subtable (trie node)
         * 2. Check the subtable default route. Exit if it exists.
         */
        ent = subtablePtr(ent);
        if ( ent.down[1].ent && (ent.down[1].ent->plen == plen) ) {
            ent = ent.down[1];
            goto AddrComp;
        }
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
 * @name   rtArtNewRoute
 *
 * @brief  API function.
 *         Allocates memory for a new route entry.
 *
 * @param[in] pt Pointer to the routing table
 *
 * @retval routeEnt* Pointer to the newly allocated route.
 * @retval NULL      There was no memory for a new route.
 */
routeEnt *
rtArtNewRoute (rtTable* pt)
{
    register routeEnt *r;

    r = (routeEnt *) calloc(1, sizeof(routeEnt));
    if ( r == NULL ) return NULL;

    return r;
}


/**
 * @name   rtArtFreeRoute
 *
 * @brief  Frees memory allocated for a route.
 *
 * @param[in] pt Pointer to the routing table
 * @param[in] r  Pointer to the route to be freed
 */
void
rtArtFreeRoute (rtTable* pt, routeEnt* r)
{
    if ( r == NULL ) {
        fprintf(stderr, "Can't delete a NULL route!\n");
        return;
    }

    free(r);
}


/**
 * @name   rtArtInsertRoute
 *
 * @brief  API function.
 *         (registered as `pt->insert()').
 *         Adds a route represented by `pEnt' to the routing table `pt'
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
rtArtInsertRoute (rtTable* pt, routeEnt* pEnt)
{
    register int l, len;
    register subtable   pst;
    register tableEntry ent;
    subtable pst2;
    u8*      pDest;
    int      index;
    u32      offset;
    bool     flag;


    assert((pt != NULL) && (pEnt != NULL));

    /*
     * Handle default route.
     */
    if ( pEnt->plen == 0 ){
        if ( pt->root[1].ent ) return pt->root[1].ent;
        pt->root[1].ent = pEnt;
        pt->nRoutes++;
        return pEnt;
    }

    index  = baseIndex(pt, pEnt->dest, pEnt->plen);
    len    = pt->psi[0].sl;     /* accumulated address bit length */
    pst    = pt->root;          /* ptr to subtable */
    l      = 0;                 /* level */
    offset = 0;
    flag   = true;
    pDest  = pEnt->dest;        /* ptr to dest. IP address */
    for (;;) {
        if ( pEnt->plen <= len ) {
            pEnt->level = l;
            return rtArtInsert(pt, pst, index, 1 << pt->psi[l].sl, flag, pEnt);
        }

        pst2 = pst;             /* save &pst[0] */
        pst += fringeIndex(&pDest, &offset, pt->psi[l].sl);
        ent = *pst;
        if ( isSubtable(ent) ) {
            ent = subtablePtr(ent);
        } else {
            ent.down  = rtArtNewSubTable(pt, l+1, ent);
            if ( ent.down == NULL ) {
                /* XXX do something later rather than panicing */
                panic(("rtArtInsertRoute: no memory"));
            }
            pst->down = makeSubtable(ent.down);
            pst2[0].count++;
        }
        pst = ent.down;         /* advance subtable ptr to next level */

        ++l;
        if ( l >= (pt->nLevels - 1) ) {
            flag = false;       /* last level */
            if ( l >= pt->nLevels ) {
                panic(("rtArtInsertRoute: shouldn't happen (l = %d)", l));
            }
        }
        len += pt->psi[l].sl;
    }
    panic(("rtArtInsertRoute: should not happen"));
}


/**
 * @name   rtArtDeleteRoute
 *
 * @brief  API function.
 *         (registered as `pt->delete()').
 *         Deletes a route represented by an IP prefix (
 *         (address and its prefix length) from the routing table.
 *         The matched route entry is freed in this function.
 *
 * @param[in] pt    Pointer to the routing table
 * @param[in] pDest Pointer to the IP address to be deleted from `pt'
 * @param[in] plen  Prefix length associated with `pDest'
 *
 * @retval ture  If the matching route is deleted from `pt'.
 *               The route entry is freed in this function.
 * @retval false If there is no matching route in `pt'.
 */
static bool
rtArtDeleteRoute (rtTable* pt, u8* pDest, int plen)
{
    register int         l, len;
    register tableEntry* pst;
    register tableEntry  ent;
    routeEnt* pEnt;
    u8*       pDest2;
    bool      flag;
    int       index;
    u32       offset;


    assert(pt && pDest);

    /*
     * Handle default route
     */
    if ( plen == 0 ) {
        pEnt = pt->root[1].ent;
        pt->root[1].ent = NULL;
        pt->nRoutes--;
        goto FreeAndReturn;
    }


    index  = baseIndex(pt, pDest, plen);
    len    = pt->psi[0].sl;     /* accumulated address bit length */
    pst    = pt->root;          /* ptr to subtable */
    l      = 0;                 /* level */
    flag   = true;
    offset = 0;
    pDest2 = pDest;             /* save dest address ptr */

    for (;;) {
        if ( plen <= len ) {
            pEnt = rtArtDelete(pt, pst, index, 1 << pt->psi[l].sl,
                               flag, pDest2, plen, l);
            goto FreeAndReturn;
        }

        pt->pTbl[l] = pst;       /* save sbutable pointer */
        pst       += fringeIndex(&pDest, &offset, pt->psi[l].sl);
        ent        = *pst;
        pt->pEnt[l] = pst;       /* save entry pointer */
        if ( isSubtable(ent) ) {
            ent = subtablePtr(ent);
            pst = ent.down;
        } else {
            return false;       /* no route */
        }

        ++l;
        if ( l >= (pt->nLevels - 1) ) {
            flag = false;       /* last level */
            if ( l >= pt->nLevels ) {
                panic(("rtArtDeleteRoute: shouldn't happen (l = %d)", l));
            }
        }
        len += pt->psi[l].sl;
    }

FreeAndReturn:
    if ( pEnt == NULL ) return false;
    rtArtFreeRoute(pt, pEnt);
    return true;
}


/**
 * @name   rtArtWalkTable
 *
 * @brief  API function.
 *         Iterates the routing table recursively
 *         (in the depth-first fashion.)
 *
 * @param[in] pt        Pointer to the routing table
 * @param[in] p         Pointer to the subtable start walk-through.
 * @param[in] i         Index to start walking in subtable `p'.
 * @param[in] threshold The first fringe index of subtable `p'.
 * @param[in] f         Callback function pointer
 * @param[in] p2        Parameter to function (*f)()
 */
void
rtArtWalkTable (rtTable* pt, subtable p, int i,
                int threshold, rtFunc f, void* p2)
{
    register routeEnt *pRt;

    if ( i < 1 ) return;        /* sanity check */

    /*
     * Fringe index handler (`i' is a fringe index)
     */
    if ( i >= threshold ) {
        if ( isSubtable(p[i]) ) {
            pRt = p[(i>>1)].ent; /* parent node (must be non fringe) */
            p = subtablePtr(p[i]).down;
            if ( p[1].ent && (pRt != p[1].ent) ) (*f)(p[1].ent, p2);
            threshold = 1 << pt->psi[p[-1].level].sl;
            rtArtWalkTable(pt, p, 1, threshold, f, p2); /* next level heap */
        } else {
            pRt = p[i].ent;
            if ( pRt && (pRt != p[(i>>1)].ent) ) (*f)(pRt, p2);
        }
        return;
    }

    /*
     * Non-fringe index handler (`i' is a non-fringe index.)
     * `p[1]' is processed by the fringe index handler
     */
    pRt = p[i].ent;
    if ( pRt && (i > 1) && (pRt != p[i>>1].ent) ) (*f)(pRt, p2);
    i <<= 1;
    rtArtWalkTable(pt, p, i, threshold, f, p2); /* lower left */
    ++i;
    rtArtWalkTable(pt, p, i, threshold, f, p2); /* lower right */
}


/**
 * @name   rtArtWalkBaseIndices
 *
 * @brief  Visit all base indices in a subtable (trie node)
 *         and call the callback function if each index has a route.
 *
 * @param[in] pt Pointer to the routing table
 * @param[in] p  Pointer to the subtable start walk-through.
 * @param[in] f  Callback function pointer
 * @param[in] p2 Parameter to function (*f)()
 */
static inline void
rtArtWalkBaseIndices (rtTable* pt, subtable p, rtFunc f, void* p2)
{
    int i, j, l, max, plen;

    l   = p[-1].level;
    max = 1 << pt->psi[l].sl;

    assert(l < pt->nLevels);

    /*
     * iterate non-fringe nodes.
     * there should not be any subtable pointers.
     * p[1].ent will be taken care of when
     * fringe nodes are visited.
     */
    plen = (l == 0) ? 0 : (pt->psi[l-1].tl + 1);
    j = 4;
    for ( i = 2; i < max; ++i ) {
        assert(!isSubtable(p[i]));

        if ( i == j ) {
            /*
             * reached the beginning of the next prefix lengh.
             */
            j <<= 1;
            ++plen;
        } else {
            assert(i < j);
        }
        if ( p[i].ent && (p[i].ent->plen == plen) ) {
            (*f)(p[i].ent, p2);
        }
    }
}


/**
 * @name  rtArtBFwalk
 *
 * @brief API function.
 *        Walks through the routing table in the breadth first fashion.
 *        Different from rtArtWalkTable(), this function is not recursive.
 *
 * @param[in] pt Pointer to the routing table
 * @param[in] p  Pointer to the beginning of
 *               subtable (trie node) to start trie walk
 * @param[in] f  Function pointer to be called with a visited route
 * @param[in] p2 Pointer parameter to (*f)().
 */
void
rtArtBFwalk (rtTable* pt, subtable p, rtFunc f, void* p2)
{
    dllHead q;
    rtArtWalkQnode* pn;
    int i, l, max, plen;


    /*
     * initialization
     */
    dllInit(&q);
    pn = calloc(1, sizeof(*pn));
    if ( !pn ) {
        return;
    }

    /*
     *  perform breadth-first iteration from `p'.
     *  pointer `p' must point to the beginning of trie node.
     */
    pn->p = p;
    dllEnqNode(&q, (dllNode*)pn);
    while ( (pn = (rtArtWalkQnode*)dllPopNode(&q)) ) {
        p = pn->p;
        free(pn);

        rtArtWalkBaseIndices(pt, p, f, p2);

        /*
         * iterate fringe nodes.
         * enqueue p[i] if it is a subtable pointer.
         */
        l    = p[-1].level;
        i    = 1 << pt->psi[l].sl;
        max  = i << 1;
        plen = pt->psi[l].tl;
        for ( ; i < max; ++i ) {
            if ( isSubtable(p[i]) ) {
                subtable  pst  = subtablePtr(p[i]).down;
                routeEnt* pEnt = pst[1].ent;
                if ( pEnt && (pEnt->plen == plen) ) {
                    /*
                     * print the route pushed out
                     * to the next level
                     */
                    (*f)(pst[1].ent, p2);
                }
                pn = calloc(1, sizeof(*pn));
                if ( !pn ) {
                    continue;
                }
                pn->p = subtablePtr(p[i]).down;
                dllEnqNode(&q, (dllNode*)pn);
            } else if ( p[i].ent ) {
                if ( p[i].ent->plen == plen ) {
                    (*f)(p[i].ent, p2);
                }
            }
        }
    }
}


/**
 * @name  rtArtDFwalk
 *
 * @brief API function.
 *        Walks through the routing table in the depth first fashion.
 *        Different from rtArtWalkTable(), this function is not recursive.
 *
 * @param[in] pt Pointer to the routing table
 * @param[in] p  Pointer to the beginning of
 *               subtable (trie node) to start trie walk.
 *               Set `p' to `pt->root' to walk through the entire table.
 * @param[in] f  Function pointer to be called with a visited route.
 *               rtArtDFwalk() clear the entire routes if `f' is NULL.
 * @param[in] p2 Pointer parameter to (*f)().
 */
void
rtArtDFwalk (rtTable* pt, subtable p, rtFunc f, void* p2)
{
    typedef struct stackNode stackNode;
    struct stackNode {
        dllNode  dlln;          /* doubly linked list node */

        subtable p;
        int dir;                /* direction (down or up) */
        int idx;                /* starting index */
        int threshold;          /* start index of fringe nodes */
    };
    enum {
        down = 0,
        up = 1,
    };

    dllHead s;
    stackNode* pn;
    int dir, i, l, threshold, plen;

    /*
     * initialization
     */
    dllInit(&s);
    pn = calloc(1, sizeof(*pn));
    if ( !pn ) {
        return;
    }

    /*
     * perform depth-first iteration from `p'.
     * Set `p' to `pt->root' to walk through the
     * entire trie.
     */
    pn->p   = p;
    pn->dir = down;
    pn->idx = 1;
    dllPushNode(&s, (dllNode*)pn);
    while ( (pn = (stackNode*)dllPopNode(&s)) ) {
        p   = pn->p;
        dir = pn->dir;
        i   = pn->idx;
        free(pn);

        l = p[-1].level;
        threshold = 1 << pt->psi[l].sl;
        assert(i < (threshold << 1));

        plen = pt->psi[l].tl - pt->psi[l].sl;
        if (i > 1) {
            int j;
            for (j = pt->psi[l].sl; i < (1 << j); --j) ;
            plen += j;
        }
        /*
         * perform depth-first iteration inside
         * trie node `p'
         */
        do {
            if ( dir == down ) {
                /*
                 * do stuff here
                 */
                if ( isSubtable(p[i]) ) {
                    assert(i >= threshold);

                    subtable  pst  = subtablePtr(p[i]).down;
                    routeEnt* pEnt = pst[1].ent;
                    if ( pEnt && (pEnt->plen == plen) ) {
                        /*
                         * treat the route pushed out
                         * to the next level
                         */
                        (*f)(pst[1].ent, p2);
                    }
                    pn = calloc(1, sizeof(*pn));
                    if ( !pn ) {
                        goto moveOn;
                    }
                    pn->p   = p;
                    if ( i & 1 ) {
                        pn->idx = i >> 1;
                        pn->dir = up;
                    } else {
                        pn->idx = i + 1;
                        pn->dir = down;
                    }
                    pn->threshold = threshold;
                    dllPushNode(&s, (dllNode*)pn);
                    pn = calloc(1, sizeof(*pn));
                    if ( !pn ) {
                        pn = (stackNode*)dllPopNode(&s);
                        free(pn);
                        goto moveOn;
                    }
                    pn->p   = pst; /* subtablePtr(p[i]).down; */
                    pn->idx = 1;
                    pn->threshold = 0;
                    dllPushNode(&s, (dllNode*)pn);
                    goto endWhile;
                } else if ( p[i].ent && (i > 1) ) {
                    if ( p[i].ent->plen >= plen ) {
                        (*f)(p[i].ent, p2);
                    }
                }
            }
moveOn:
            /*
             * advance index `i'
             */
            if ( i < threshold ) {
                if ( dir == up ) {
                    if ( i & 1 ) {
                        i >>= 1;
                        --plen;
                    } else {
                        ++i;
                        dir = down;
                    }
                } else {
                    i <<= 1;
                    ++plen;
                }
            } else {
                if ( i & 1 ) {
                    i >>= 1;
                    --plen;
                    dir = up;
                } else {
                    ++i;
                }
            }
        } while (i != 1);       /* start is always 1 */
endWhile:
        ;
    }
}


/**
 * @struct prefixTbl
 *
 * @brief Structure used by `collectRoutes()'.
 *        `p' must be a pointer equivalent to
 *        `routeEnt entry[number_of_routes]'.
 *
 * @var   i
 * @brief The number of routes collected up to now.
 * @var   p
 * @brief Must be initialized like
 *        `calloc(number_of_routes, sizeof(routeEnt))
 */
typedef struct prefixTbl {
    int i;
    routeEnt* p;
} prefixTbl;


/**
 * @name  collectRoutes
 *
 * @brief Callback function for `rtArtFlushRoutes()'.
 *        Delete all the route entries in the routing table.
 *
 * @param[in] pt Pointer to the routing table
 */
static void
collectRoutes (routeEnt* p, void* p2)
{
    prefixTbl* pt = (prefixTbl*)p2;
    routeEnt* pEnt = pt->p + pt->i; /* &(pt->p[pt->i]) */

    ++pt->i;
    memcpy(pEnt->dest, p->dest, sizeof(pEnt->dest));
    pEnt->plen  = p->plen;
    pEnt->level = p->level;
}

/**
 * @name  rtArtFlushRoutes
 *
 * @brief API Function.
 *        (registered as `pt->flush()').
 *        Delete all the route entries in the routing table.
 * @param[in] pt Pointer to the routing table
 *
 * @retval ture  All route entries are successfully deleted.
 * @retval false Otherwise.
 */
bool
rtArtFlushRoutes (rtTable* pt)
{
    prefixTbl t;                /* all prefixes are stored in `t' */
    bool rc = true;
    int i, n;
    n = pt->nRoutes;
    t.i = 0;
    t.p = calloc(n, sizeof(routeEnt));
    if (!t.p) {
        return false;
    }
    /*
     * Pass 1. Collect all prefixes.
     */
    rtArtDFwalk(pt, pt->root, collectRoutes, &t);
    /*
     * Pass 2. Delete them.
     */
    for (i = 0; i < n; ++i) {
        if (!pt->delete(pt, t.p[i].dest, t.p[i].plen)) {
            rc = false;
            assert(1);
        }
    }
    free(t.p);
    return rc;
}


/**
 * @name  rtArtDestroy
 *
 * @brief API Function.
 *        (registered as `pt->deleteTable()').
 *        Delete all the route entries in the routing table, then
 *        Free the routing table itself.
 * @param[in,out] p Pointer to the pointer to `rtTable'. `*p' is
 *                  set to NULL at the end of this function.
 */
static void
rtArtDestroy (rtTable** p)
{
    rtTable* pt = *p;

    pt->flush(pt);
    free(pt->root - 1);
    free(pt->pTbl);
    free(pt->pEnt);
    free(pt->psi);
    free(pt);
    *p = NULL;
}


/**
 * @name  rtArtInit
 *
 * @brief API Function.
 *        Creates and initializes a routing table.
 *        Example usage:
 *          - IPv4 routing table
 *          - The stride lengths are:
 *             level 0: 16 bits
 *             level 1:  8 bits
 *             level 2:  8 bits
 *          - Use a path-compressed trie
 *
 *         s8      sl[3] = { 16, 8, 8 };
 *         rtTable pt    = rtArtInit(3, sl, 32, pathCompTrie);
 *
 * @param[in] nLevels The number of trie node levels
 *                    (or the number of stride lengths)
 * @param[in] psl     Pointer to an array of stride lengths
 * @param[in] alen    Bit length of IP addresses (32 or 128)
 * @param[in] type    simpleTrie (0) or pathCompTrie (1).
 *
 * @retval rtTable* Pointer to the allocated routing table
 * @retval NULL     Failed to allocate a new routing table
 */
rtTable *
rtArtInit (int nLevels, s8* psl, int alen, trieType type)
{
    register rtTable *pt;
    register int i;
    int sum;

    pt = calloc(1, sizeof(rtTable));
    if ( pt == NULL ) return NULL;

    pt->nLevels = nLevels;
    pt->alen    = alen;              /* address length */
    pt->len     = bits2bytes(alen);  /* alen in bytes */
    pt->off     = -1 - bytes2nPtrs((alen >> 3));

    pt->psi = calloc(nLevels, sizeof(strideInfo));
    if ( pt->psi == NULL ) goto tblFree;

    sum = 0;
    for ( i = 0; i < nLevels; ++i ) {
        pt->psi[i].sl = psl[i];
        pt->psi[i].sb = sum >> 3;
        pt->psi[i].bo = sum & 7;
        sum += psl[i];
        pt->psi[i].tl = sum;
    }

    assert(sum == alen);

    if ( type == pathCompTrie ) {
        return rtArtPcInit(pt);
    }

    pt->root = rtArtRootTable(pt);
    if ( pt->root == NULL ) goto slFree;

    pt->pEnt = calloc(nLevels, sizeof(subtable));
    if ( pt->pEnt == NULL ) goto rootFree;

    pt->pTbl = calloc(nLevels, sizeof(subtable*));
    if ( pt->pTbl == NULL ) goto entFree;

    pt->insert         = rtArtInsertRoute;
    pt->delete         = rtArtDeleteRoute;
    pt->deleteTable    = rtArtDestroy;
    pt->flush          = rtArtFlushRoutes;
    pt->findMatch      = rtArtFindMatch;
    pt->findExactMatch = rtArtFindExactMatch;

    return pt;
    assert(1);                  /* should not happen */


entFree:
    free(pt->pEnt);
rootFree:
    free(pt->root);
slFree:
    free(pt->psi);
tblFree:
    free(pt);
    return NULL;
}

#if 0
/**
 * @name   heapVisit
 *
 * @brief  Visits all elements in a heap in the depth first fashion.
 *         This function is *NOT* recursive.
 *
 * @param[in] start     Start index
 * @param[in] threshold The first index of the bottom of the hearp
 */
void
heapDFvisit (int start, int threshold)
{
    enum {
        down = 0,
        up   = 1,
    };
    int i = start;
    int dir = down;

    do {
        /*
         * do something with `i'
         */
        if (dir == down) {
            printf("%d\n", i);
        }

        if (i < threshold) {
            if (dir == up) {
                if (i & 1) {
                    i >>= 1;
                } else {
                    ++i;
                    dir = down;
                }
            } else {
                i <<= 1;
            }
        } else {
            if (i & 1) {
                i >>= 1;
                dir = up;
            } else {
                ++i;
            }
        }
    } while (i != start);
}

#endif//0
