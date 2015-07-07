#ifndef __Types_h
#define __Types_h

#include <sys/types.h>
#include <stdio.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t  u8;
typedef int64_t   s64;
typedef int32_t   s32;
typedef int16_t   s16;
typedef char      s8;

typedef u32 ipv4a;              /* in the host byte order */
typedef u32 ipv4na;             /* in the network byte order */


typedef enum {
    success = 1,
    fail    = 0
} result;

typedef enum {
    true  = 1,
    false = 0
} bool, boolean;


#define panic(_str_) \
{ \
    printf("\nPANIC!! at %s, line %d: ", __FILE__, __LINE__); \
    printf _str_; \
    printf("\n"); \
    abort(); \
}

#endif /* __Types_h */
