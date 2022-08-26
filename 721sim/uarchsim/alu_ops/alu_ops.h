#ifndef ALU_OPS_H
#define ALU_OPS_H

#include <vector>
#include <cassert>
#include <algorithm>
#include "decode.h"
#include "trap.h"
#include "payload.h"

typedef reg_t (*alu_op_func_t)(payload_t &pay_buf, const state_t &state);

struct alu_op_desc_t {
  uint32_t match;
  uint32_t mask;
  alu_op_func_t alu_op_fn;
};

class alu_ops_t {
private:
  std::vector<alu_op_desc_t> instructions;
  std::vector<alu_op_desc_t *> opcode_map;
  std::vector<alu_op_desc_t> opcode_store;
public:
  alu_ops_t();

  alu_op_func_t get_alu_op_fn(insn_t insn);

  void register_insn(alu_op_desc_t desc);

  void build_opcode_map();
};


#endif //ALU_OPS_H
