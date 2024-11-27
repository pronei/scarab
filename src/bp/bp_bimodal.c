#include "bp/bp.h"
#include "bp/bp_bimodal.h"
#include "libs/hash_lib.h"
#include "globals/utils.h"
#include "globals/assert.h"
#include <stdlib.h>
#include <string.h>

static Hash_Table* bp_bimodal_hist_table_all_cores;

void bp_bimodal_init(void) {
    bp_bimodal_hist_table_all_cores = (Hash_Table*) malloc(sizeof(Hash_Table) * NUM_CORES);
    for (uns i = 0; i < NUM_CORES; i++) {
        Hash_Table bp_bimodal_hist_table = bp_bimodal_hist_table_all_cores[i];
        init_hash_table(&bp_bimodal_hist_table, "Bimodal Predictor History",
                        BIMODAL_TABLE_SIZE, sizeof(unsigned char));

        for(uns i = 0; i < BIMODAL_TABLE_SIZE; i++) {
          Flag           new_entry;
          unsigned char* state = hash_table_access_create(
            &bp_bimodal_hist_table, i, &new_entry);
          ASSERT(0, state);
          if(new_entry) {
            *state = STRONGLY_TAKEN;
          }
        }
    }
}

#define PROC_IDX(proc_id) (proc_id % NUM_CORES)

uns8 bp_bimodal_pred(Op* op) {
    Hash_Table bp_bimodal_hist_table = bp_bimodal_hist_table_all_cores[PROC_IDX(op->proc_id)];
    Addr key = convert_to_cmp_addr(op->proc_id, op->inst_info->addr);
    unsigned char* state = hash_table_access(&bp_bimodal_hist_table, key);

    if (!state) {return TAKEN;}

    if (*state >= WEAKLY_TAKEN) {
        return TAKEN;
    } else {
        return NOT_TAKEN;
    }
}

void bp_bimodal_update(Op* op) {
    Hash_Table bp_bimodal_hist_table = bp_bimodal_hist_table_all_cores[PROC_IDX(op->proc_id)];
    Addr key = convert_to_cmp_addr(op->proc_id, op->inst_info->addr);
    Flag new_entry;
    unsigned char* state = hash_table_access_create(&bp_bimodal_hist_table, key, &new_entry);

    if (new_entry) {*state = STRONGLY_TAKEN;}

    if (op->oracle_info.dir == TAKEN) {
        if (*state < STRONGLY_TAKEN) {
            (*state)++;
        }
    } else {
        if (*state > STRONGLY_NOT_TAKEN) {
            (*state)--;
        }
    }
}
