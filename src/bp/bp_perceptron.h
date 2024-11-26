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

#endif