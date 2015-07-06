#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Types.h"


/* 1. buf must have 16 byte space
   2. add must be in the host byte order
 */
char *
inet_h2a(ipv4a add, char *buf)
{
    static char s[16];

    if ( !buf ) {
        buf = s;
    }

    sprintf(buf, "%d.%d.%d.%d",
            (add >> 24) & 0xff,
            (add >> 16) & 0xff,
            (add >> 8) & 0xff,
            add & 0xff);

    return buf;
}

/* 1. buf must have 16 byte space
   2. add must be in the network byte order
 */
char *
inet_n2a(ipv4na add, char *buf)
{
    static char s[16];

    if ( !buf ) {
        buf = s;
    }

    sprintf(buf, "%d.%d.%d.%d",
            add & 0xff,
            (add >> 8) & 0xff,
            (add >> 16) & 0xff,
            (add >> 24) & 0xff);

    return buf;
}


void
inet_a2n(int ver, char* s, u8* p)
{
    char* s2;
    ipv4a ipa;
    int   i;

    if ( ver == 4 ) {
        ipa = ntohl(inet_addr(s));
        p[0] = ipa >> 24;
        p[1] = (ipa >> 16) & 0xff;
        p[2] = (ipa >> 8) & 0xff;
        p[3] = ipa & 0xff;
        return;
    }
    s2 = s;
    ver = 1;                    /* user ver for flag */
    for ( i = 0; ver; ++i ) {
        for (;;) {
            if ( *s2 == '\0' ) {
                ver = 0;
                break;
            }
            if ( *s2 == ':' ) {
                *s2 = '\0';
                break;
            }
            ++s2;
        }
        p[i] = strtoul(s, NULL, 16);
        if ( ver ) {
            *s2 = ':';
        } else {
            return;
        }
        ++s2;
        s = s2;
    }
}


/* 1. buf must have 18 byte space
   2. add must be in the network byte order
 */
char *
inetStr(u8* add, int ver, char *buf)
{
    static char s[18];

    if ( !buf ) {
        buf = s;
    }

    switch ( ver ) {
    case 4:
        sprintf(buf, "%d.%d.%d.%d", add[0], add[1], add[2], add[3]);
        break;
    case 6:
        sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                add[0], add[1], add[2], add[3], add[4], add[5]);
        break;
    default:
        buf = NULL;
        break;
    }

    return buf;
}


static s8 magic[37] = {
     0,  5,  9, 30,  6, 31, 32,  0, 14, 13,
    24, 12, 27, 23, 18, 11,  0, 26, 20, 22,
     3, 17,  1, 10,  7,  0, 15, 25, 28, 19,
     0, 21,  4,  2,  8, 16, 29
};

int
mask2plen(ipv4a mask)
{
    return magic[mask % 37];
}
