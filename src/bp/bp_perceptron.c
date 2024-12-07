#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/utils.h"

#include "libs/hash_lib.h"
#include "bp/bp.h"
#include "bp/bp.param.h"
#include "bp/bp_perceptron.h"

#include "statistics.h"

static Bp_Perceptron_Data** bp_percep_data_all_cores = NULL;

void perceptron_timestamp(Op* op) {}
void perceptron_spec_update(Op* op) {}
void perceptron_retire(Op* op) {}
void perceptron_recover(Recovery_Info* info) {}
uns8 perceptron_full(uns proc_id) { return 0; }

void perceptron_bp_init(void) {
    bp_percep_data_all_cores = (Bp_Perceptron_Data**) malloc(sizeof(Bp_Perceptron_Data*) * NUM_CORES);
    for (uns c = 0; c < NUM_CORES; c++) {
        bp_percep_data_all_cores[c] = (Bp_Perceptron_Data*) malloc(sizeof(Bp_Perceptron_Data));
        Bp_Perceptron_Data* bp_percep_data = bp_percep_data_all_cores[c];

        bp_percep_data->perceptron_global_hist = 0;
        bp_percep_data->history_to_tables = (Hash_Table*) malloc(sizeof(Hash_Table) * PERCEP_GRID_DIM_1);
        
        init_hash_table(&bp_percep_data->table, "Percep_hash_table",
                        PERCEP_TABLE_SIZE, sizeof(Perceptron));

        for(uns i = 0; i < PERCEP_GRID_DIM_1; i++) {
          char table_name[64];
          snprintf(table_name, sizeof(table_name), "Hist_percep_hash_table_%d", i);
          init_hash_table(&bp_percep_data->history_to_tables[i], table_name,
                          PERCEP_TABLE_SIZE, sizeof(Perceptron));

        }
    }
}

#define PROC_IDX(proc_id) (proc_id % NUM_CORES)
#define PERCEPTRON_DIM_IDX(addr, dim_entries) ((addr) & N_BIT_MASK(LOG2(dim_entries)))
#define TRUNCATE_TO_INT64(u) ((int64)((u) & 0x7FFFFFFFFFFFFFFF))

Perceptron* lazy_access_create(Hash_Table* table, int64 key) {
  Flag new_entry;
  Perceptron* p = (Perceptron*)hash_table_access_create(table, key, &new_entry);
  if(new_entry) {
    p->weights = (int32*)malloc(sizeof(int32) * (PERCEP_HIST_LEN + 1));
    for(uns k = 0; k < PERCEP_HIST_LEN + 1; k++) {
      p->weights[k] = PERCEP_WEIGHT_INIT_VAL;
    }
  }
  return p;
}

Perceptron* get_perceptron(Op* op, Bp_Perceptron_Data* bp_percep_data) {
    Addr  branch_addr = op->oracle_info.pred_addr;
    uns64 hist        = bp_percep_data->perceptron_global_hist;
    Perceptron* p;
    Hash_Table* percep_table;

    switch (BP_PERCEP_MODEL) {
      case 1:
      default:
        // branchPC =(hash)> perceptron
        p = lazy_access_create(&bp_percep_data->table, branch_addr);
        break;
      case 2:
        // hist =(mod)> branchPC =(hash)> perceptron
        percep_table = &bp_percep_data
             ->history_to_tables[PERCEPTRON_DIM_IDX(hist, PERCEP_GRID_DIM_1)];
        p = lazy_access_create(percep_table, branch_addr);
        break;
      case 3:
        // hist ^ branchPC =(hash)> perceptron
        p = lazy_access_create(&bp_percep_data->table, hist ^ branch_addr);
        break;
    }

    return p;
}

uns8 perceptron_predict(Op* op) {
    uns8  output;
    int32 dot_product = 0;
    Bp_Perceptron_Data* bp_percep_data = bp_percep_data_all_cores[PROC_IDX(op->proc_id)];
    uns64 hist        = bp_percep_data->perceptron_global_hist;
    
    Perceptron* p = get_perceptron(op, bp_percep_data);
    int32* w = &(p->weights[0]);

    // start from bias
    dot_product = *(w++);

    // compute the dot product
    for (uns i = 0; i < PERCEP_HIST_LEN; i++, w++) {
        if (hist & (1ULL << i))
            dot_product += *w;
        else
            dot_product -= *w;
    }

    output = dot_product >= 0 ? 1 : 0;

    DEBUG(op->proc_id,
           "hist:%s dot_product:%d output:%d \n", hexstr64(hist), dot_product, output);

    // need to add to state so we can access this during training
    op->perceptron_dot_output = dot_product;

    return output;
}

void perceptron_update(Op* op) {
    if (op->table_info->cf_type != CF_CBR) {
        return;
    }

    int32 dot_product  = op->perceptron_dot_output;
    Flag  y            = dot_product >= 0 ? 1 : 0;
    Flag  t            = op->oracle_info.dir;

    Bp_Perceptron_Data* bp_percep_data = bp_percep_data_all_cores[PROC_IDX(op->proc_id)];
    uns64 hist = bp_percep_data->perceptron_global_hist;
    Perceptron* p = get_perceptron(op, bp_percep_data);
    
    int32* w = &(p->weights[0]);

    // don't train if the branch was correctly predicted and the output
    // falls outside [-theta, theta]
    Flag do_train = y != t || abs(dot_product) <= PERCEPTRON_THRESH;

    if (do_train) {
        // update bias based upon whether branch was mispredicted or not
        if (t)
            (*w)++;
        else
            (*w)--;
        if (CAP_PERCEP_WEIGHTS) {
          if(*w > MAX_WEIGHT_PERCEP)
            *w = MAX_WEIGHT_PERCEP;
          else if(*w < MIN_WEIGHT_PERCEP)
            *w = MIN_WEIGHT_PERCEP;
        }

        // increment w_i if x_i positively correlates with the true direction
        for (uns i = 0; i < PERCEP_HIST_LEN; i++, w++) {
            int32 old_w = *w;
            UNUSED(old_w);
            Flag match = (hist & (1ULL << i)) > 0;
            if (match == t) {
                (*w)++;
                if (CAP_PERCEP_WEIGHTS) {
                  if(*w > MAX_WEIGHT_PERCEP)
                    *w = MAX_WEIGHT_PERCEP;
                }
            } else {
                (*w)--;
                if (CAP_PERCEP_WEIGHTS) {
                  if(*w < MIN_WEIGHT_PERCEP)
                    *w = MIN_WEIGHT_PERCEP;
                }
            }
            DEBUG(op->proc_id,
                  "hist:%s old_dot_product:%d correct_pred:%d *w[%d]: %d->%d\n",
                  hexstr64(hist), dot_product, y==t, i, old_w, *w);
        }

        
    }

    bp_percep_data->perceptron_global_hist <<= 1;
    bp_percep_data->perceptron_global_hist |= op->oracle_info.dir;
    // Max history length is 63
    bp_percep_data->perceptron_global_hist &= ((1ULL << PERCEP_HIST_LEN) - 1);
}