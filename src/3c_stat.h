#ifndef __3C_STAT_H__
#define __3C_STAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdbool.h>

#include "libs/cache_lib.h"

typedef enum {
    HIT,
    COMPULSORY_MISS,
    CAPACITY_MISS,
    CONFLICT_MISS
} MissType;

MissType classify_miss(Addr addr, Cache* dcache, Cache* conflict_cache);

#ifdef __cplusplus
}
#endif

#endif
