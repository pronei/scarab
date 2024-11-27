#include "debug/debug_macros.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/utils.h"

#include "bp/bp.h"
#include "bp/bp_perceptron.h"

#include "statistics.h"

// 16 by 1024 grid of perceptrons
// indexed first by 4 global history bits followed by branch PC
// TODO: drive from params
#define GRID_DIM_1 16
#define GRID_DIM_2 1024
#define PERCEP_HIST_LEN 32

#define PERCEP_WEIGHT_INIT_VAL 0

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BP, ##args)

static Bp_Perceptron_Data** bp_percep_data_all_cores = NULL;

#define CMP_ADDR_MASK_CUS (((Addr)-1) << 10)

// used to add 3 bits of proc_id to the 10 bit addr
// TODO: needs GRID_DIM_2 to be 2^13
// Addr convert_to_cmp_addr_13bit(uns8 proc_id, Addr addr) {
//   if((addr & CMP_ADDR_MASK_CUS)) {
//     addr = addr & ~CMP_ADDR_MASK_CUS;
//   }
//   return (addr | (((Addr)proc_id) << 10)) & N_BIT_MASK(13);
// }

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
        bp_percep_data->grid = (Perceptron**)malloc(sizeof(Perceptron*) * GRID_DIM_1);

        for(uns i = 0; i < GRID_DIM_1; i++) {
          bp_percep_data->grid[i] = (Perceptron*)malloc(sizeof(Perceptron) * GRID_DIM_2);

          for(uns j = 0; j < GRID_DIM_2; j++) {
            // number of weights defined plus bias
            bp_percep_data->grid[i][j].weights = (int32*)malloc(
              sizeof(int32) * (PERCEP_HIST_LEN + 1));

            for(uns k = 0; k < PERCEP_HIST_LEN + 1; k++) {
              bp_percep_data->grid[i][j].weights[k] = PERCEP_WEIGHT_INIT_VAL;
            }
          }
        }
    }
}

#define PROC_IDX(proc_id) (proc_id % NUM_CORES)
#define PERCEPTRON_DIM_IDX(addr, dim_entries) ((addr) & N_BIT_MASK(LOG2(dim_entries)))

uns8 perceptron_predict(Op* op) {
    uns8  output;
    int32 dot_product = 0;
    Addr  branch_addr = convert_to_cmp_addr(op->proc_id, op->inst_info->addr);
    uns32 index1      = PERCEPTRON_DIM_IDX(op->oracle_info.pred_global_hist, GRID_DIM_1);
    uns32 index2      = PERCEPTRON_DIM_IDX(branch_addr, GRID_DIM_2);
    uns32 hist        = op->oracle_info.pred_global_hist;
    uns32 mask;
    uns   ii;
    
    Bp_Perceptron_Data* bp_percep_data = bp_percep_data_all_cores[PROC_IDX(op->proc_id)];
    Perceptron* p = &(bp_percep_data->grid[index1][index2]);
    int32* w = &(p->weights[0]);

    // start from bias
    dot_product = *(w++);

    // compute the dot product
    for (mask = ((uns32)1) << 31, ii = 0; ii < PERCEP_HIST_LEN;
         ii++, mask >>= 1, w++) {
        if (!!(hist & mask))
            dot_product += *w;
        else
            dot_product += -(*w);
    }

    output = dot_product > 0 ? 1 : 0;

    DEBUG(op->proc_id,
           "index1:%d index2:%d hist:%s dot_product:%d output:%d \n",
           index1, index2, hexstr64(hist), dot_product, output);

    // TODO: need to add stat event if necessary

    // need to add to state so we can access this during training
    op->perceptron_dot_output = dot_product;

    return output;
}

#define PERCEPTRON_THRESH (int32) (1.93 * (PERCEP_HIST_LEN) + 14)
// used from bp_conf.c, 127 & -128
#define MAX_WEIGHT ((1 << (8 - 1)) - 1)
#define MIN_WEIGHT (-(MAX_WEIGHT + 1))

void perceptron_update(Op* op) {
    // TODO: check if we need to update GHR ourselves or whether bp.c does it for us...

    if (op->table_info->cf_type != CF_CBR) {
        return;
    }

    Addr  branch_addr  = convert_to_cmp_addr(op->proc_id, op->inst_info->addr);
    uns32 index1       = PERCEPTRON_DIM_IDX(op->oracle_info.pred_global_hist, GRID_DIM_1);
    uns32 index2       = PERCEPTRON_DIM_IDX(branch_addr, GRID_DIM_2);
    uns32 hist         = op->oracle_info.pred_global_hist;
    int32 dot_product  = op->perceptron_dot_output;
    int8  correct_pred = op->oracle_info.mispred ? -1 : 1;
    uns8  do_train     = 1;
    uns32 mask;
    uns   ii;

    Bp_Perceptron_Data* bp_percep_data = bp_percep_data_all_cores[PROC_IDX(op->proc_id)];
    Perceptron* p = &(bp_percep_data->grid[index1][index2]);
    int32* w = &(p->weights[0]);

    // don't train if the branch was correctly predicted and the output
    // falls outside [-theta, theta]
    if (dot_product > PERCEPTRON_THRESH && correct_pred == 1) {
        do_train = 0;
    } else if (dot_product < -PERCEPTRON_THRESH && correct_pred == 1) {
        do_train = 0;
    }

    if (do_train) {
        // update bias based upon whether branch was taken or not
        if (op->oracle_info.dir)
            (*w)++;
        else
            (*w)--;
        if (*w > MAX_WEIGHT)
            *w = MAX_WEIGHT;
        else if (*w < MIN_WEIGHT)
            *w = MIN_WEIGHT;

        // increment w_i if x_i positively correlates with the true direction
        for (mask = ((uns32)1) << 31, ii = 0; ii < PERCEP_HIST_LEN;
            ii++, mask >>= 1, w++) {
            
            int32 old_w = *w;
            UNUSED(old_w);

            if (!!(hist & mask) == op->oracle_info.dir) {
                (*w)++;
                if (*w > MAX_WEIGHT)
                    *w = MAX_WEIGHT;
            } else {
                (*w)--;
                if (*w < MIN_WEIGHT)
                    *w = MIN_WEIGHT;
            }
            DEBUG(op->proc_id,
                  "index1:%d index2:%d hist:%s old_dot_product:%d correct_pred:%d *w[%d]: %d->%d\n",
                  index1, index2, hexstr64(hist), dot_product, correct_pred, ii, old_w, *w);
        }
    }
}