#ifndef __BP_BIMODAL_H__
#define __BP_BIMODAL_H__

#include "bp/bp.h"
#include "globals/global_types.h"

#define STRONGLY_TAKEN 3
#define WEAKLY_TAKEN 2
#define WEAKLY_NOT_TAKEN 1
#define STRONGLY_NOT_TAKEN 0

#define BIMODAL_TABLE_SIZE (1 << 22)

void bp_bimodal_init(void);
uns8 bp_bimodal_pred(Op*);
void bp_bimodal_update(Op*);

#endif