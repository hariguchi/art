#ifndef __DLLIST_H_
#define __DLLIST_H_

/* $Id: dllist.h,v 1.1 2015/06/25 21:19:27 cvsremote Exp $

   dllist.h: doubly linked list library


   Copyright (c) 2011, Yoichi Hariguchi
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       o Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       o Redistributions in binary form must reproduce the above
         copyright notice, this list of conditions and the following
         disclaimer in the documentation and/or other materials provided
         with the distribution.
       o Neither the name of the Yoichi Hariguchi nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#ifndef OPTIMIZATION_ON
#pragma GCC diagnostic ignored "-Wunused-function"
#endif /* OPTIMIZATION_ON */


/*
 * Doubly linked list definitions
 */
typedef struct dllNode_ {
    struct dllNode_* next;
    struct dllNode_* prev;
} dllNode;
typedef struct dllHead_ {
    dllNode* head;
    dllNode* tail;
} dllHead;


static inline void
dllInit (dllHead* ph)
{
    ph->head = ph->tail = NULL;
}
static inline void
dllPushNode (dllHead* ph, dllNode* pn)
{
    if (!ph->head) {
        pn->next = pn->prev = NULL;
        ph->head = ph->tail = pn;
        return;
    }
    ph->head->prev = pn;
    pn->next = ph->head;
    pn->prev = NULL;
    ph->head = pn;
}
static inline dllNode*
dllPopNode (dllHead* ph)
{
    dllNode* pn;

    pn = ph->head;
    if (!pn) {
        return NULL;
    }
    if (pn == ph->tail) {
        ph->head = ph->tail = NULL;
        return pn;
    }
    ph->head = pn->next;
    ph->head->prev = NULL;
    return pn;
}
static inline void
dllEnqNode (dllHead* ph, dllNode* pn)
{
    if (!ph->tail) {
        pn->next = pn->prev = NULL;
        ph->head = ph->tail = pn;
        return;
    }
    pn->next       = NULL;
    pn->prev       = ph->tail;
    ph->tail->next = pn;
    ph->tail       = pn;
}
static inline dllNode*
dllRmLastNode (dllHead* ph)
{
    dllNode* pn;

    pn = ph->tail;
    if (!pn) {
        return NULL;
    }
    if (pn == ph->head) {
        ph->head = ph->tail = NULL;
        return pn;
    }
    ph->tail       = pn->prev;
    pn->prev->next = NULL;
    return pn;
}
static inline void
dllPrependNode (dllHead* ph, dllNode* pn, dllNode* pnNew)
{
    if (ph->head == pn) {
        return dllPushNode(ph, pnNew);
    }
    pnNew->next    = pn;
    pnNew->prev    = pn->prev;
    pn->prev->next = pnNew;
    pn->prev       = pnNew;
}
static inline void
dllAppendNode (dllHead* ph, dllNode* pn, dllNode* pnNew)
{
    if (ph->tail == pn) {
        return dllEnqNode(ph, pnNew);
    }
    pnNew->next    = pn->next;
    pnNew->prev    = pn;
    pn->next->prev = pnNew;
    pn->next       = pnNew;
}

static inline void
dllMergeLists (dllHead* ph1, dllHead* ph2)
{
    ph1->tail->next = ph2->head;
    ph1->tail       = ph2->tail;
    ph2->head       = NULL;
    ph2->tail       = NULL;
}

static inline void
dllSplitList (dllHead* ph1, dllHead* ph2, dllNode* pn)
{
    if (pn->prev == NULL) {
        ph2->head = pn;
        ph2->tail = ph1->tail;
        ph1->head = NULL;
        ph1->tail = NULL;
        return;
    }
    ph2->head = pn;
    ph2->tail = ph1->tail;
    ph1->tail = pn->prev;
    pn->prev  = NULL;
    ph1->tail->next = NULL;
}

#endif /* __DLLIST_H_ */
