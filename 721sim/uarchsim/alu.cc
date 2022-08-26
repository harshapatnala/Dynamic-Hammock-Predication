#include <cmath>
#include "pipeline.h"
#include "trap.h"
#include "mulhi.h"
#include "softfloat.h"
#include "platform.h" // softfloat isNaNF32UI, etc.
#include "internals.h" // ditto

// Helper functions at the bottom of this file.
static void ExecMULT(reg_t, reg_t,
                     reg_t*, reg_t*);
static void ExecMULTU(reg_t, reg_t,
                      reg_t*, reg_t*);
static void ExecSRA(reg_t, unsigned int, reg_t*);
static void ExecSRAV(reg_t, reg_t, reg_t*);


void pipeline_t::agen(unsigned int index) {
	insn_t inst;
	reg_t addr;

	// Only loads and stores should do AGEN.
	assert(IS_MEM_OP(PAY.buf[index].flags));

	inst = PAY.buf[index].inst;

	// Execute the AGEN.
  if(IS_AMO(PAY.buf[index].flags)){
    //AMO ops do not use the displacement addressing
  	addr = PAY.buf[index].A_value.dw;
  } else if(IS_LOAD(PAY.buf[index].flags)){
    //Loads use the I-type immediate encoding
  	addr = PAY.buf[index].A_value.dw + inst.i_imm();
  } else {
    //Stores use the S-type immediate encoding
  	addr = PAY.buf[index].A_value.dw + inst.s_imm();
  }
	PAY.buf[index].addr = addr;

	//// Adjust address of the lower half of DLW and DSW.
	//if ((inst.opcode() == DLW) || (inst.opcode() == DSW)) {
	//	assert(PAY.buf[index].split);
	//	if (!PAY.buf[index].upper) {
	//		PAY.buf[index].addr += 4;
	//	}
	//}
}	// agen()


void pipeline_t::alu(unsigned int index) {
	//Do the MUXing for CMOV Instruction.
	if(PAY.buf[index].instruction_type == CMOV) {
		PAY.buf[index].C_value.dw = (PAY.buf[index].D_value.dw == 1) ? PAY.buf[index].B_value.dw : PAY.buf[index].A_value.dw; //Predicate set--> Take B value.
		return;
	}

  auto& pay_buf = PAY.buf[index];
	insn_t insn = pay_buf.inst;
  auto alu_op_fn = alu_ops.get_alu_op_fn(insn);
  state_t& state = *get_state();
  alu_op_fn(pay_buf, state);

  if(PAY.buf[index].branch && PAY.buf[index].branch_type == HAMMOCK) {
	  PAY.buf[index].C_value.dw = (PAY.buf[index].c_next_pc != INCREMENT_PC(PAY.buf[index].pc)) ? 1 : 0; //Next PC is not the computed PC-->Taken Path-->Set Predicate.
  }
}
