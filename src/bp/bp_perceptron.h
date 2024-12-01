#ifndef __BP_PERCEP_H__
#define __BP_PERCEP_H__

#include "bp/bp.h"
#include "libs/hash_lib.h"
#include "globals/global_types.h"

// TODO: required for hist[N:] -> branchPC -> perceptron (or) hist[N:] -> hist xor branchPC -> perceptron approaches
// typedef struct Perceptrons_Hash_Table_struct {
//     Hash_Table table;
// } Perceptrons_Hash_Table;

typedef struct Bp_Perceptron_Data_struct {
    Perceptron** grid;
    // Perceptrons_Hash_Table* history_to_tables;
    Hash_Table table;
    uns64 perceptron_global_hist;
} Bp_Perceptron_Data;

Perceptron* get_perceptron(Op*, Bp_Perceptron_Data*);

void perceptron_bp_init(void);
uns8 perceptron_predict(Op*);
void perceptron_update(Op*);
void perceptron_timestamp(Op*);
void perceptron_spec_update(Op*);
void perceptron_retire(Op*);
void perceptron_recover(Recovery_Info*);
uns8 perceptron_full(uns);

#endif