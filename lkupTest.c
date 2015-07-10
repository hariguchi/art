#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipArt.h"

enum {
    SHOWTBLALL = '0',
    SHOWTBL    = '1',
    INSPECT    = '2',
    LOOKUP     = '3',
    ADD        = '4',
    DELETE     = '5',
    LOAD       = '6',
    UNLOAD     = '7',
    LKUP_TEST  = '8',
    MAKE_TBL   = '9',
    EXIT       = 0x71,

    MAX_LEVEL  = 32,
};


typedef struct {
    u8* pStart;                 /* start address ptr */
    u8* pEnd;                   /* end address ptr */
    int len;                    /* address bit length */
} range;

typedef void (*pInspect)(rtTable* pt, routeEnt* pEnt, int l, u8* pDef);

void    mkRtTbl();
void    rmRtTbl();
void    showMenu();
void    lookUpRoute(int af);
void    lookupTest(rtTable *pt);
void    addRoute();
void    delRoute();
boolean getSearchPerf();
void    printRoutes(rtTable *pt);
void    printRtTableRange(rtTable* pt);
void    rtInspect(rtTable* pt, pInspect f);
void    rtPcInspect(rtTable* pt);
void    inspectNode (rtTable* pt, routeEnt* pEnt, int l, u8* pDef);
void    inspectPcNode (rtTable* pt, routeEnt* pEnt, int l, u8* pDef);

void    prRoute(routeEnt* p, void* p2);
bool    prefixCheck(routeEnt* pent, u8* dest);



rtTable *Ptable = NULL;         /* routing table pointer */

int NxitHeap[MAX_LEVEL];        /* transit heap stats */


static inline int
getAf (rtTable* pt)
{
    switch (pt->alen) {
    case 32:
        return AF_INET;
    case 128:
        return AF_INET6;
    default:
        panic(("wrong address length: %d", pt->alen));
        return 0;
    }
}


static inline int
getVer (rtTable* pt)
{
    switch (pt->alen) {
    case 32:
        return 4;
    case 128:
        return 6;
    default:
        panic(("wrong address length: %d", pt->alen));
        return 0;
    }
}


#define USAGE \
"Usage: rtLookup <4|6> <pc|simple> [batch | [stride length ...]]\n"
void
usage (void)
{
    fprintf(stderr, USAGE);
    exit(1);
}



boolean
insRoute (u8* dest, int plen, u8* nh, u32 cid)
{
    routeEnt *p;
    int len;

    p = rtArtNewRoute(Ptable);
    if ( p == NULL ) {
        fprintf(stderr, "insRoute: Out of memory\n");
        exit(1);
    }
    len = (Ptable->alen == 32) ? 4 : 16;
    memcpy(p->dest, dest, len);
    p->plen  = plen;
    if ( Ptable->insert(Ptable, p) == p ) return true;
    return false;
}


rtTable*
defineTable (int ver, trieType type, char* sl)
{
    char buf[128];
    char *p;
    int i, j;
    int sum;


    if (ver == 4) {
                sl[0] = 4;
                sl[1] = 4;
                sl[2] = 4;
                sl[3] = 4;
                sl[4] = 4;
                sl[5] = 4;
                sl[6] = 4;
                sl[7] = 4;
                i     = 8;
#if 0
        sl[0] = 16;
        sl[1] = 8;
        sl[2] = 8;
        i   = 3;
#endif/*0*/
        sum = 32;
    } else {
        sl[0] = 16;
        memset(sl + 1, 4, 28);
        i   = 29;
        sum = 128;
    }
    printf("stride length (default is");
    for ( j = 0; j < i; ++j ) {
        printf(" %d", sl[j]);
    }
    printf("): ");
    if (!fgets(buf, 128, stdin)) {
        return NULL;
    }
    if ( *buf == '\n' ) return rtArtInit(i, sl, sum, type);

    sum = 0;
    p = strtok(buf, " ");
    for (i = 0; ;++i) {
        if (p == NULL) break;

        sl[i] = strtoul(p, NULL, 0);
        sum += sl[i];
        p = strtok(NULL, " ");
    }
    if (i == 0) return NULL;

    if (((ver == 4) && (sum != 32)) || ((ver == 6) && (sum != 48))) {
        fprintf(stderr, "wrong stride lengths (sum = %d)\n", sum);
        exit(1);
    }

    return rtArtInit(i, sl, sum, type);
}


void
showMenu ()
{
    printf("\n%4d: show entire routing table\n", SHOWTBLALL - '0');
    printf("%4d: show routing table\n", SHOWTBL - '0');
    printf("%4d: inspect the routing table\n", INSPECT - '0');
    printf("%4d: look up a route\n", LOOKUP - '0');
    printf("%4d: add a route\n", ADD - '0');
    printf("%4d: delete a route\n", DELETE - '0');
    printf("%4d: load routes\n", LOAD - '0');
    printf("%4d: unload all routes\n", UNLOAD - '0');
    printf("%4d: lookup test (exact match and LPM)\n", LKUP_TEST - '0');
    printf("%4d: make table\n", MAKE_TBL - '0');
    printf("%4c: exit\n", EXIT);
    printf("Select item: ");
}


int
main (int argc, char *argv[])
{
    int      af, c, sum, ver;
    char     buf[8];
    char     sl[32];
    trieType type;


    if (argc <= 2) usage();
    ver = strtoul(argv[1], NULL, 0);
    switch (ver) {
    case 4:
        af = AF_INET;
        break;
    case 6:
        af = AF_INET6;
        break;
    default:
        usage();
        break;
    }
    if (!strcmp(argv[2], "pc")) {
        type = pathCompTrie;
    } else {
        type = simpleTrie;
    }

    if (argc > 3) {
        /* batch mode. print out performance and quit.
         */
        if (argc < 5) {
            if ( ver == 4 ) {
                sl[0] = 4;
                sl[1] = 4;
                sl[2] = 4;
                sl[3] = 4;
                sl[4] = 4;
                sl[5] = 4;
                sl[6] = 4;
                sl[7] = 4;
                c     = 8;
#if 0
                sl[0] = 16;
                sl[1] = 8;
                sl[2] = 8;
                c   = 3;
#endif/*0*/
                sum = 32;
            } else {
                sl[0] = 16;
                sl[1] = 4;
                sl[2] = 4;
                sl[3] = 4;
                sl[4] = 4;
                sl[5] = 4;
                sl[6] = 4;
                sl[7] = 4;
                sl[8] = 4;
                c   = 9;
                sum = 48;
            }
        } else {
            sum = 0;
            for (c = 0; c < 32; ++c) {
                if (4 + c >= argc) break;

                sl[c] = strtoul(argv[4+c], NULL, 0);
                sum += sl[c];
            }
        }
        if ( ((ver == 4) && (sum != 32)) || ((ver == 6) && (sum != 48)) ) {
            fprintf(stderr, "wrong stride lengths (sum = %d)\n", sum);
            exit(1);
        }
        Ptable = rtArtInit(c, sl, sum, type);
        if ( Ptable == NULL ) {
            fprintf(stderr, "can't alloc table\n");
            exit(1);
        }
        if ( getSearchPerf() == false ) {
            exit(1);
        }
        exit(0);
    }

    /* interactive mode
     */
    while(1) {
        showMenu();
        if (fgets(buf, 8, stdin) == NULL) {
            exit(0);
        }
        if (*buf == EXIT) exit(0);

        c = atoi(buf) + '0';
        if ((Ptable == NULL)
            && (c != MAKE_TBL) && (c != LOAD) && (c != EXIT)) {
            printf("Routing table does not exist.\n");
            continue;
        }

        switch (c) {
          case SHOWTBLALL:
            printRoutes(Ptable);
            break;
          case SHOWTBL:
            printRtTableRange(Ptable);
            break;
          case INSPECT:
              if ( type == pathCompTrie ) {
                  rtInspect(Ptable, inspectPcNode);
                  rtPcInspect(Ptable);
              } else {
                  rtInspect(Ptable, inspectNode);
              }
            break;
          case LOOKUP:
            lookUpRoute(af);
            break;
          case ADD:
            addRoute();
            break;
          case DELETE:
            delRoute();
            break;
          case LOAD:
            if (Ptable == NULL) {
                Ptable = defineTable(ver, type, sl);
                assert(Ptable);
            }
            mkRtTbl();
            break;
          case UNLOAD:
              if (Ptable == NULL) {
                printf("Routing table does not exist\n");
              } else {
                  rmRtTbl();
              }
              break;
        case LKUP_TEST:
            lookupTest(Ptable);
            break;
          case MAKE_TBL:
            if (Ptable) {
                printf("Routing table already exists\n");
                break;
            }
            Ptable = defineTable(ver, type, sl);
            assert(Ptable);
            break;
          default:
            break;
        }
    }
    exit(0);
    return 0;                   /* to make GCC quiet */
}


void
lookUpRoute (int af)
{
    char      sIpa[32];
    u8        ipa[16];
    pRouteEnt pRtEnt;


    printf("destination: ");
    if (!fgets(sIpa, 32, stdin)) {
        return;
    }
    if ( inet_pton(af, sIpa, ipa) != 1 ) {
        fprintf(stderr, "Error: inet_pton(): %s\n", sIpa);
        return;
    }
    pRtEnt = Ptable->findMatch(Ptable, ipa);
    printf("Key:   %s\n", sIpa);
    if ( pRtEnt ) {
        if ( !inet_ntop(af, pRtEnt->dest, sIpa, sizeof(sIpa)) ) {
            if ( af == AF_INET ) {
                fprintf(stderr, "Error: inet_ntop(): %d.%d.%d.%d/%d\n",
                        pRtEnt->dest[0], pRtEnt->dest[1], 
                        pRtEnt->dest[2], pRtEnt->dest[3], pRtEnt->plen);
            } else {
                fprintf(stderr,
                        "Error: inet_ntop(): "
                        "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                        "%02x%02x:%02x%02x:%02x%02x:%02x%02x/%d\n",
                        pRtEnt->dest[0], pRtEnt->dest[1], pRtEnt->dest[2],
                        pRtEnt->dest[3], pRtEnt->dest[4], pRtEnt->dest[5],
                        pRtEnt->dest[6], pRtEnt->dest[7], pRtEnt->dest[8],
                        pRtEnt->dest[9], pRtEnt->dest[10], pRtEnt->dest[11],
                        pRtEnt->dest[12], pRtEnt->dest[13], pRtEnt->dest[14],
                        pRtEnt->dest[15], pRtEnt->plen);
            }
            return;
        }
        printf("Route: %s/%d\n", sIpa, pRtEnt->plen);
    } else {
        printf("Route: no route for the key\n");
    }
}


void
printRtTableRange (rtTable* p)
{
    int   af;
    char  buf[32];
    u8    start[16];
    u8    end[16];
    range r;
    char  *s;

    af = getAf(p);
    printf("start: ");
    if (!fgets(buf, 32, stdin)) {
        return;
    }
    s = index(buf, '\n');
    if ( s ) {
        *s = '\0';
    }
    if ( inet_pton(af, buf, start) != 1 ) {
        fprintf(stderr, "Error: inet_pton(): %s\n", buf);
        return;
    }
    printf("end: ");
    if (!fgets(buf, 32, stdin)) {
        return;
    }
    s = index(buf, '\n');
    if ( s ) {
        *s = '\0';
    }
    if ( inet_pton(af, buf, end) != 1 ) {
        fprintf(stderr, "Error: inet_pton(): %s\n", buf);
        return;
    }

    r.pStart = start;
    r.pEnd   = end;
    r.len    = p->alen;

    rtArtWalkTable(p, p->root, 1, 1 << p->psi[0].sl, prRoute, &r);
}


void
mkRtTbl (void)
{
    FILE *fp;
    char* p;
    char  buf[128];
    u8    dest[16];
    int   af;
    int   plen = 0;


    if (Ptable->alen == 32) {
        af  = AF_INET;
        strcpy(buf, "data/v4routes-random1.txt");
        
    } else {
        af  = AF_INET6;
        strcpy(buf, "data/v6routes-random1.txt");
    }

    if ((fp = fopen(buf, "r")) == NULL) {
        printf("No such file: %s\n", buf);
        exit(1);
    }

    while (fgets(buf, sizeof(buf), fp)) {
        p = index(buf, '/');
        if (p) {
            *p = '\0';
            if (inet_pton(af, buf, dest) != 1) {
                fprintf(stderr, "Error: inet_pton(): %s\n", buf);
                continue;
            }
            plen = strtol(p+1, NULL, 10);
        }
        if (insRoute(dest, plen, 0, 0) == false) {
            panic(("mkRtTbl: can't add route: %s/%d\n", buf, plen));
        }
    }
    fclose(fp);
}


void
rmRtTbl (void)
{
    FILE  *fp;
    char   buf[128];
    u8     dest[16];
    int    af;
    int    plen = 0;
    char*  p;


    if (Ptable->alen == 32) {
        af  = AF_INET;
        strcpy(buf, "data/v4routes-random3.txt");
    } else {
        af  = AF_INET6;
        strcpy(buf, "data/v6routes-random2.txt");
    }

    if ((fp = fopen(buf, "r")) == NULL) {
        printf("No such file: %s\n", buf);
        exit(1);
    }

    Ptable->nSubtblFreed = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        p = index(buf, '/');
        if (p) {
            *p = '\0';
            if (inet_pton(af, buf, dest) != 1) {
                fprintf(stderr, "Error: inet_pton(): %s\n", buf);
                continue;
            }
            plen = strtol(p+1, NULL, 10);
        }
        if (!Ptable->delete(Ptable, dest, plen)) {
            panic(("rmRtTbl: can't rm route: %s/%d\n", buf, plen));
        }
    }
    fclose(fp);
    printf("%d subtables were freed.\n", Ptable->nSubtblFreed);
}


#ifdef SEARCH_TEST
#define V4FILE "data/v4routes-random2.txt"
#define V6FILE "data/v6routes-random2.txt"
#else /* SEARCH_TEST */
#define V4FILE "ipa.txt"
#define V6FILE "v6ipa.txt"
#endif /* SEARCH_TEST */

boolean
getSearchPerf (void)
{
    return true;
}


void
addRoute (void)
{
    u8        dest[16];
    int       plen;
    int       af;
    char      buf[32];
    routeEnt *pEnt;
    routeEnt *p;
    char     *s;

    af = getAf(Ptable);
    printf("IP Version is %d\nprefix (addr/plen): ", getVer(Ptable));
    if (!fgets(buf, 32, stdin)) {
        return;
    }
    s = index(buf, '/');
    if (s) {
        *s = '\0';
        if (inet_pton(af, buf, dest) != 1) {
            fprintf(stderr, "Error: inet_pton(): %s\n", buf);
            return;
        }
        plen = strtol(s+1, NULL, 10);
    } else {
        fprintf(stderr, "Error: wrong format.\n");
        return;
    }
    if ( plen > Ptable->alen ) {
        printf("wrong prefix length (%d). 0 <= prefix length <= %d\n",
               plen, Ptable->alen);
    }

    pEnt = rtArtNewRoute(Ptable);
    memcpy(pEnt->dest, &dest, Ptable->alen/8);
    pEnt->plen = plen;

    p = Ptable->insert(Ptable, pEnt);
    if ( p == NULL ) printf("No more memory\n");
    if ( p != pEnt ) printf("Same prefix already exists\n");
}


void
delRoute (void)
{
    u8   dest[16];
    int  plen;
    int  af;
    char buf[32];
    char* s;


    af = getAf(Ptable);
    printf("IP Version is %d\nprefix (addr/plen): ", getVer(Ptable));
    if (!fgets(buf, 32, stdin)) {
        return;
    }
    s = index(buf, '/');
    if (s) {
        *s = '\0';
        if (inet_pton(af, buf, dest) != 1) {
            fprintf(stderr, "Error: inet_pton(): %s\n", buf);
            return;
        }
        plen = strtol(s+1, NULL, 10);
    } else {
        fprintf(stderr, "Error: wrong format.\n");
        return;
    }
    if (Ptable->delete(Ptable, dest, plen) == false) {
        printf("no such route\n");
    }
}


void
prRoute (routeEnt *p, void* p2)
{
    int blen;                   /* byte length */
    range* pr = p2;
    char s[INET6_ADDRSTRLEN];
    int af;

    assert(p2);

    blen = pr->len >> 3;
    if ( pr->pStart ) {
        if ( memcmp(p->dest, pr->pStart, blen) < 0 ) return;
        if ( memcmp(p->dest, pr->pEnd, blen) > 0 ) return;
    }
    af = (blen == 4) ? AF_INET : AF_INET6;
    if (!inet_ntop(af, p->dest, s, sizeof(s))) {
        perror("inet_ntop()");
    } else {
        printf("%s/%d\n", s, p->plen);
    }
}


void
printRoutes (rtTable *p)
{
    range r;

    r.pStart = r.pEnd = NULL;
    r.len = p->alen;
#if 0
    rtArtWalkTrie(p, p->root, prRoute, &r);
#endif
    rtArtWalkTable(p, p->root, 1, 1 << p->psi[0].sl, prRoute, &r);
}

void
inspectPcNode (rtTable* pt, routeEnt* pEnt, int l, u8* pDef)
{
    char buf[128];


    if ( l != pEnt->level ) {
        snprintf(buf, sizeof(buf),
                 "%d.%d.%d.%d/%d",
                 pEnt->dest[0],
                 pEnt->dest[1],
                 pEnt->dest[2],
                 pEnt->dest[3], pEnt->plen);
        fprintf(stderr, "level mismatch: %s: %d, %d\n",
                buf, l, pEnt->level);
    }
    if ( l > 0 ) {
        if ( !cmpAddr(pDef, pEnt->dest, pt->psi[l-1].tl) ) {
            snprintf(buf, sizeof(buf),
                     "route: %d.%d.%d.%d/%d, pDef: %d.%d.%d.%d",
                     pEnt->dest[0], pEnt->dest[1],
                     pEnt->dest[2], pEnt->dest[3], pEnt->plen,
                     pDef[0], pDef[1], pDef[2], pDef[3]);
            fprintf(stderr,
                    "def addr mismatch: level %d: %s", l, buf);
        }
    }
}

void
inspectNode (rtTable* pt, routeEnt* pEnt, int l, u8* pDef)
{
    char buf[128];


    if ( l != pEnt->level ) {
        snprintf(buf, sizeof(buf),
                 "%d.%d.%d.%d/%d",
                 pEnt->dest[0],
                 pEnt->dest[1],
                 pEnt->dest[2],
                 pEnt->dest[3], pEnt->plen);
        fprintf(stderr, "level mismatch: %s: %d, %d\n",
                buf, l, pEnt->level);
    }
}


#if __SIZEOF_POINTER__ == 8
# define COUNT_MISMATCH    "# of routes mismatch: %s, %d, %lu\n"
# define NROUTES_MISMATCH  "# of routes mismatch: %s, %d, %u\n"
# define NSUBTBLS_MISMATCH "# of subtables mismatch: %s, %d, %u\n"
#elif __SIZEOF_POINTER__ == 4
# define COUNT_MISMATCH    "# of routes mismatch: %s, %d, %u\n"
# define NROUTES_MISMATCH  "# of routes mismatch: %s, %d, %u\n"
# define NSUBTBLS_MISMATCH "# of subtables mismatch: %s, %d, %u\n"
#else
#error Unsupported pointer size
#endif /* __SIZEOF_POINTER__ */

void
rtInspect (rtTable* pt, pInspect f)
{
    dllHead  q;
    subtable p;
    rtArtWalkQnode* pn;
    routeEnt* pEnt;
    u8* pDef;
    u32 nNodes;
    u32 nRoutes;
    int i, j, l, max, plen;
    int sCnt, rCnt;
    char buf[128];


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
    pn->p = pt->root;
    dllEnqNode(&q, (dllNode*)pn);
    nNodes  = 0;
    nRoutes = 0;
    while ( (pn = (rtArtWalkQnode*)dllPopNode(&q)) ) {
        p = pn->p;
        free(pn);
        ++nNodes;
        sCnt = 0;
        rCnt = 0;

        l   = p[-1].level;
        max = 1 << pt->psi[l].sl;

        assert(l < pt->nLevels);

        /*
         * iterate non-fringe nodes.
         * there should not be any subtable pointers.
         * p[1].ent will be taken care of when
         * fringe nodes are visited.
         */
        plen = (l == 0) ? 1 : (pt->psi[l-1].tl + 1);
        j = 4;
        pDef = (u8*)(p + pt->off);
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
            pEnt = p[i].ent;
            if ( pEnt && (pEnt->plen == plen) ) {
                ++nRoutes;      /* increment the number of routes */
                ++rCnt;
                (*f)(pt, pEnt, l, pDef);
            }
        }
        /*
         * iterate fringe nodes.
         * enqueue p[i] if it is a subtable pointer.
         */
        plen = pt->psi[l].tl;
        max <<= 1;
        for ( ; i < max; ++i ) {
            if ( isSubtable(p[i]) ) {
                subtable  pst  = subtablePtr(p[i]).down;
                routeEnt* pEnt = pst[1].ent;
                ++sCnt;
                if ( pEnt && (pEnt->plen == plen) ) {
                    /*
                     * print the route pushed out
                     * to the next level
                     */
                    ++nRoutes;
                    ++rCnt;
                    (*f)(pt, pEnt, l, pDef);
                }
                pn = calloc(1, sizeof(*pn));
                if ( !pn ) {
                    continue;
                }
                pn->p = subtablePtr(p[i]).down;
                dllEnqNode(&q, (dllNode*)pn);
            } else if ( p[i].ent ) {
                pEnt = p[i].ent;
                if ( pEnt->plen == plen ) {
                    ++nRoutes;
                    ++rCnt;
                    (*f)(pt, pEnt, l, pDef);
                }
            }
        }
        if ( f == inspectPcNode ) {
            if ( rCnt != p[0].nRoutes ) {
                snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                         pDef[0], pDef[1], pDef[2], pDef[3]);
                fprintf(stderr,
                        NROUTES_MISMATCH, buf, rCnt, p[0].nRoutes);
            }
            if ( sCnt != p[0].nSubtables ) {
                snprintf(buf, sizeof(buf),
                         "%d.%d.%d.%d",
                         pDef[0], pDef[1], pDef[2], pDef[3]);
                fprintf(stderr,
                        NSUBTBLS_MISMATCH, buf, sCnt, p[0].nSubtables);
            }
        } else {
            if ( (rCnt + sCnt) != p[0].count ) {
                snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                         pDef[0], pDef[1], pDef[2], pDef[3]);
                fprintf(stderr,
                        COUNT_MISMATCH, buf, rCnt + sCnt, p[0].count);
            }
        }
    }
    printf("\n%d routes. %d nodes.\n", nRoutes, nNodes);
}

bool
prefixCheck (routeEnt* pent, u8* dest)
{
    int len;
    u8  mask;
    u8* p1;

    p1 = pent->dest;

    for ( len = 8; len <= pent->plen; len += 8 ) {
        if ( *p1++ != *dest++ ) return false;
    }
    if ( len == pent->plen ) return true;

    /* Check remining bits < 8
     */
    mask = ~((1 << (len - pent->plen)) - 1);
    if ( (*p1 & mask) != (*dest & mask) ) return false;

    return true;
}

static void
walk (rtTable* pt, subtable p, int i, int threshold, int n, u32* stat)
{
    register routeEnt *pEnt;

    if ( i < 1 ) return;        /* sanity check */

    /*
     * Fringe index handler (`i' is a fringe index)
     */
    if ( i >= threshold ) {
        if ( isSubtable(p[i]) ) {
            pEnt = p[(i>>1)].ent; /* parent node (must be non fringe) */
            p = subtablePtr(p[i]).down;
            if ( p[1].ent && (pEnt != p[1].ent) ) {
                assert(p[1].ent->level >= n);
                ++stat[p[1].ent->level - n];
            }
            threshold = 1 << pt->psi[p[-1].level].sl;
            walk(pt, p, 1, threshold, ++n, stat); /* next level subtable */
        } else {
            pEnt = p[i].ent;
            if ( pEnt && (pEnt != p[(i>>1)].ent) ) {
                assert(p[i].ent->level >= n);
                ++stat[p[i].ent->level - n];
            }
        }
        return;
    }

    /*
     * Non-fringe index handler (`i' is a non-fringe index.)
     * `p[1]' is processed by the fringe index handler
     */
    pEnt = p[i].ent;
    if ( pEnt && (i > 1) && (pEnt != p[i>>1].ent) ) {
        assert(p[i].ent->level >= n);
        ++stat[p[i].ent->level - n];
    }
    i <<= 1;
    walk(pt, p, i, threshold, n, stat); /* lower left */
    ++i;
    walk(pt, p, i, threshold, n, stat); /* lower right */
}

void
rtPcInspect (rtTable* pt)
{
    u32* stat;
    u32 sum;
    int i;


    stat = calloc(pt->nLevels, sizeof(*stat));
    if ( !stat ) {
        fprintf(stderr, "Error: no memory\n");
        exit(1);
    }
    walk(pt, pt->root, 1, 1 << pt->psi[0].sl, 0, stat);
    sum = 0;
    printf("\n");
    for ( i = 0; i < pt->nLevels; ++i ) {
        sum += stat[i];
        printf("%2d: %8d\n", i, stat[i]);
    }
    printf("\nTotal: %d\n", sum);
}

void
lookupTest (rtTable* pt)
{
    routeEnt* pEnt;
    routeEnt* pEnt2;
    FILE *fp;
    char* p;
    char  buf[128];
    u8    dest[16];
    int   af;
    int   plen = 0;


    if ( pt->alen == 32 ) {
        af  = AF_INET;
        strcpy(buf, "data/v4routes-random1.txt");
        
    } else {
        af  = AF_INET6;
        strcpy(buf, "data/v6routes-random1.txt");
    }

    if ( (fp = fopen(buf, "r")) == NULL ) {
        printf("No such file: %s\n", buf);
        exit(1);
    }

    while ( fgets(buf, sizeof(buf), fp) ) {
        p = index(buf, '/');
        if ( p ) {
            *p = '\0';
            if ( inet_pton(af, buf, dest) != 1 ) {
                fprintf(stderr, "Error: inet_pton(): %s\n", buf);
                continue;
            }
            plen = strtol(p+1, NULL, 10);
        }
        pEnt = pt->findExactMatch(pt, dest, plen);
        if ( !pEnt ) {
            fprintf(stderr, "Error: failed to find route: %s\n", buf);
            continue;
        }
        if ( af == AF_INET ) {
            if ( plen < 32 ) {
            ++dest[3];
            }
            if ( !inet_ntop(af, dest, buf, sizeof(buf)) ) {
                perror("inet_ntop()");
            }
        } else {
            if ( plen < 128 ) {
                ++dest[15];
            }
            if ( !inet_ntop(af, dest, buf, sizeof(buf)) ) {
                perror("inet_ntop()");
            }
        }
        pEnt2 = pt->findMatch(pt, dest);
        if ( !pEnt2 ) {
            fprintf(stderr,
                    "Error: failed to find matching route: %s\n", buf);
            continue;
        }
        if ( pEnt2->plen < pEnt->plen ) {
            fprintf(stderr, "Error: failed longest prefix matching\n");
            if ( !inet_ntop(af, pEnt2->dest, buf, sizeof(buf)) ) {
                perror("1: inet_ntop()");
            }
            fprintf(stderr, "  matched: %s\n", buf);
            if ( !inet_ntop(af, pEnt->dest, buf, sizeof(buf)) ) {
                perror("2: inet_ntop()");
            }
            fprintf(stderr, "  longer:  %s\n", buf);
        }
            
    }
}
