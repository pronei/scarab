#ifndef __BP_BIMODAL_H__
#define __BP_BIMODAL_H__

#include "bp/bp.h"
#include "globals/global_types.h"

#define STRONGLY_TAKEN 3
#define WEAKLY_TAKEN 2
#define WEAKLY_NOT_TAKEN 1
#define STRONGLY_NOT_TAKEN 0

#define BIMODAL_TABLE_SIZE (1 << 12)

void bp_bimodal_init(void);
uns8 bp_bimodal_pred(Op*);
void bp_bimodal_update(Op*);
void bp_bimodal_timestamp(Op*);
void bp_bimodal_spec_update(Op*);
void bp_bimodal_retire(Op*);
void bp_bimodal_recover(Recovery_Info*);
uns8 bp_bimodal_full(uns);

#endif