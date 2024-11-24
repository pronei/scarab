#include "bp/bp.h"
#include "globals/global_defs.h"

// 16 by 1024 grid of perceptrons
// indexed first by 4 global history bits followed by branch PC
// TODO: drive from params
#define GRID_DIM_1 16
#define GRID_DIM_2 1024
#define PERCEP_HIST_LEN 32

#define PERCEP_WEIGHT_INIT_VAL 0

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_BP, ##args)

typedef struct Bp_Perceptron_Data_struct {
    Perceptron** grid;
    uns64 perceptron_global_hist;
} Bp_Perceptron_Data;

static Bp_Perceptron_Data* bp_percep_data = NULL;

#define CMP_ADDR_MASK_CUS (((Addr)-1) << 10)
// used to add 3 bits of proc_id to the 10 bit addr
// TODO: needs GRID_DIM_2 to be 2^13
Addr convert_to_cmp_addr_13bit(uns8 proc_id, Addr addr) {
  if((addr & CMP_ADDR_MASK_CUS)) {
    addr = addr & ~CMP_ADDR_MASK_CUS;
  }
  return (addr | (((Addr)proc_id) << 10)) & N_BIT_MASK(13);
}

void perceptron_bp_init(void) {
    bp_percep_data = (Bp_Perceptron_Data *) malloc(sizeof(Bp_Perceptron_Data));
    bp_percep_data->grid = (Perceptron**) malloc(sizeof(Perceptron*) * GRID_DIM_1);

    for (uns i = 0; i < GRID_DIM_1; i++) {
        bp_percep_data->grid[i] = (Perceptron*) malloc(sizeof(Perceptron) * GRID_DIM_2);
        for (uns j = 0; j < GRID_DIM_2; j++) {
            bp_percep_data->grid[i][j].weights = (int32*) malloc(sizeof(int32) * PERCEP_HIST_LEN+1);
            for (uns k = 0; k < PERCEP_HIST_LEN+1; k++) {
                bp_percep_data->grid[i][j].weights[k] = PERCEP_WEIGHT_INIT_VAL;
            }
        }
    }
}

#define PERCEPTRON_DIM_IDX(addr, dim_entries) ((addr) & N_BIT_MASK(LOG2(dim_entries)))

uns8 perceptron_predict(Op* op) {
    uns8  output;
    int32 dot_product = 0;
    // TODO: if num_cores > 1, then change to custom
    Addr  branch_addr = convert_to_cmp_addr(op->proc_id, op->inst_info->addr);
    uns32 index1      = PERCEPTRON_DIM_IDX(op->oracle_info.pred_global_hist, GRID_DIM_1);
    uns32 index2      = PERCEPTRON_DIM_IDX(branch_addr, GRID_DIM_2);
    uns32 hist        = op->oracle_info.pred_global_hist;
    uns32 mask;
    uns   ii;
    
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

    return output;
}

void perceptron_update(Op* op) {
    int32 output      = 0;
    // TODO: if num_cores > 1, then change to custom
    Addr  branch_addr = convert_to_cmp_addr(op->proc_id, op->inst_info->addr);
    uns32 index1      = PERCEPTRON_DIM_IDX(op->oracle_info.pred_global_hist, GRID_DIM_1);
    uns32 index2      = PERCEPTRON_DIM_IDX(branch_addr, GRID_DIM_2);
    uns32 hist        = op->oracle_info.pred_global_hist;
}