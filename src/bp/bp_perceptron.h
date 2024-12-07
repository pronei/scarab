#ifndef __BP_PERCEP_H__
#define __BP_PERCEP_H__

#include "debug/debug_macros.h"
#include "bp/bp.h"
#include "bp/bp.param.h"
#include "libs/hash_lib.h"
#include "globals/global_types.h"

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BP, ##args)

#define PERCEP_WEIGHT_INIT_VAL 0

#define PERCEPTRON_THRESH (int32) (1.93 * (PERCEP_HIST_LEN) + 14)
#define MAX_WEIGHT_PERCEP ((1 << (LOG2(PERCEPTRON_THRESH)+1)) - 1)
#define MIN_WEIGHT_PERCEP (-(MAX_WEIGHT_PERCEP + 1))

typedef struct Bp_Perceptron_Data_struct {
    Perceptron** grid;
    Hash_Table* history_to_tables;
    Hash_Table table;
    uns64 perceptron_global_hist;
} Bp_Perceptron_Data;

Perceptron* get_perceptron(Op*, Bp_Perceptron_Data*);
Perceptron* lazy_access_create(Hash_Table*, int64);

void perceptron_bp_init(void);
uns8 perceptron_predict(Op*);
void perceptron_update(Op*);
void perceptron_timestamp(Op*);
void perceptron_spec_update(Op*);
void perceptron_retire(Op*);
void perceptron_recover(Recovery_Info*);
uns8 perceptron_full(uns);

#endif