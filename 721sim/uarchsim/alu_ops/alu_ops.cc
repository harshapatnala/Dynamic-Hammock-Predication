#include "alu_ops.h"

#define REGISTER_INSN(a, name, match, mask) \
  extern reg_t alu_rv64_##name(payload_t &pay_buf, const state_t &state); \
  a->register_insn((alu_op_desc_t){match, mask, alu_rv64_##name});

static reg_t alu_op_do_nothing(payload_t &pay_buf, const state_t &state) {
  int xlen = 64;
  reg_t pc = pay_buf.pc;
  insn_t insn = pay_buf.inst;
  reg_t npc = sext_xlen(pc + insn_length(insn.opcode()));
  pay_buf.fflags = 0;
  pay_buf.c_next_pc = npc;
  return npc;
}

alu_ops_t::alu_ops_t() {
  #define DECLARE_INSN(name, match, mask) REGISTER_INSN(this, name, match, mask)

  #include "alu_ops_declare.h"

  #undef DECLARE_INSN
  build_opcode_map();
}

alu_op_func_t alu_ops_t::get_alu_op_fn(insn_t insn) {
  size_t mask = opcode_map.size() - 1;
  alu_op_desc_t *desc = opcode_map[insn.bits() & mask];

  while ((insn.bits() & desc->mask) != desc->match)
    desc++;

  return desc->alu_op_fn;
}

void alu_ops_t::register_insn(alu_op_desc_t desc) {
  assert(desc.mask & 1);
  instructions.push_back(desc);
}

void alu_ops_t::build_opcode_map() {
  size_t buckets = -1;
  for (auto &inst : instructions)
    while ((inst.mask & buckets) != buckets)
      buckets /= 2;
  buckets++;

  struct cmp {
    decltype(alu_op_desc_t::match) mask;

    cmp(decltype(mask) mask) : mask(mask) {}

    bool operator()(const alu_op_desc_t &lhs, const alu_op_desc_t &rhs) {
      if ((lhs.match & mask) != (rhs.match & mask))
        return (lhs.match & mask) < (rhs.match & mask);
      return lhs.match < rhs.match;
    }
  };
  std::sort(instructions.begin(), instructions.end(), cmp(buckets - 1));

  opcode_map.resize(buckets);
  opcode_store.resize(instructions.size() + 1);

  size_t j = 0;
  for (size_t b = 0, i = 0; b < buckets; b++) {
    opcode_map[b] = &opcode_store[j];
    while (i < instructions.size() && b == (instructions[i].match & (buckets - 1)))
      opcode_store[j++] = instructions[i++];
  }

  assert(j == opcode_store.size() - 1);
  opcode_store[j].match = opcode_store[j].mask = 0;
  opcode_store[j].alu_op_fn = &alu_op_do_nothing;
}
