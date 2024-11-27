#ifndef __BP_PERCEP_H__
#define __BP_PERCEP_H__

#include "bp/bp.h"
#include "globals/global_types.h"

typedef struct Bp_Perceptron_Data_struct {
    Perceptron** grid;
    uns64 perceptron_global_hist;
} Bp_Perceptron_Data;

void perceptron_bp_init(void);
uns8 perceptron_predict(Op*);
void perceptron_update(Op*);
void perceptron_timestamp(Op*);
void perceptron_spec_update(Op*);
void perceptron_retire(Op*);
void perceptron_recover(Recovery_Info*);
uns8 perceptron_full(uns);

#endif