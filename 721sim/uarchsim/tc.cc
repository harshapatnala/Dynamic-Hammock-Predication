#include <cinttypes>
#include <cassert>
#include <cmath>

#include "processor.h"
#include "mmu.h"
#include "decode.h"
#include "config.h"

#include "fetchunit_types.h"
#include "tc.h"


tc_t::tc_t(bool perfect, mmu_t *mmu, uint64_t max_cb, uint64_t max_length) {
   this->perfect = perfect;
   this->mmu = mmu;
   this->max_cb = max_cb;
   this->max_length = max_length;
}

tc_t::~tc_t() {
}

// Inputs:
// pc: The start pc of the fetch bundle.
// cb_predictions: A uint64_t packed with "m" 2-bit counters for predicting conditional branches.
// ib_predicted_target: Predicted target for a jump indirect or call indirect, if the fetch bundle ends with this type of branch instruction.
// ras_predicted_target: Predicted target for a return, if the fetch bundle ends with this type of branch instruction.
//
// this->max_length: "n", the maximum number of instructions in a fetch bundle.
// this->max_cb: "m", the maximum number of conditional branches allowed in a fetch bundle.
//
// Outputs:
// 1. bundle[]: The predicted fetch bundle.
//    For each instruction slot in the fetch bundle, the trace cache sets nine fields (valid, branch, branch_type, branch_target, pc, next_pc, exception, exception_cause, and insn).
// 2. *update: This is a data structure needed by the Fetch Unit to speculatively update its predictors (BHRs, RAS) and pc.
//    In particular, the Fetch Unit needs to know:
//    - The predicted PC of the next fetch bundle, to update its pc.
//    - How many conditional branches are in the assembled fetch bundle, to know how many predictions to shift into its BHRs.
//    - Whether or not it needs to pop the RAS.
//    - Whether or not it needs to push the RAS, and, if so, which pc to push onto the RAS.
bool tc_t::lookup(uint64_t pc, uint64_t cb_predictions, uint64_t ib_predicted_target, uint64_t ras_predicted_target, fetch_bundle_t bundle[], spec_update_t *update) {
   insn_t insn;
   bool taken;
   uint64_t pos = 0;
   uint64_t num_cond_branch = 0;
   bool terminated = false;

   assert(perfect);	// FIX_ME: add a real trace cache, too.

   // Initialize these two fields in the "update" variable (which is needed by the Fetch Unit to speculatively update its predictors and pc).
   // Initially assume the fetch bundle doesn't end in a call (push_ras) or return (pop_ras) instruction, and set to true if and when we determine that it does.
   update->pop_ras = false;
   update->push_ras = false;

   while ((pos < max_length) && !terminated) {
      // This instruction in the bundle is valid.
      bundle[pos].valid = true;

      // Each instruction in the bundle carries with it, its full pc.
      bundle[pos].pc = pc;

      // Try fetching the instruction via the MMU.
      // Generate a "NOP with fetch exception" if the MMU reference generates an exception.
      bundle[pos].exception = false;
      try {
         bundle[pos].insn = (mmu->load_insn(pc)).insn;
      }
      catch (trap_t& t) {
	 bundle[pos].exception = true;
         bundle[pos].exception_cause = t.cause();
         bundle[pos].insn = insn_t(INSN_NOP);
	 terminated = true;	// Terminate the fetch bundle at the offending instruction.
      }

      // The variable "insn" is assumed to exist by some macros.
      insn = bundle[pos].insn;

      // (0) Determine if the instruction is a branch.  If it is a branch, determine its type and taken target.
      // (1) Determine the instruction's next_pc field (i.e., pc of the next instruction, which may be in the same bundle or at the start of the next bundle).
      // (2) Determine if this is the last instruction in the bundle.
      //     End the fetch bundle at the maximum number of conditional branches, or at call direct, jump indirect, call indirect, or return.
      // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
      switch (bundle[pos].insn.opcode()) {
         case OP_BRANCH:
	    // Increment the number of conditional branches in the fetch bundle.
	    num_cond_branch++;

	    // The low two bits of cb_predictions correspond to the next two-bit counter to examine (because we shift it right, subsequently).
	    // From this two-bit counter, set the taken flag, accordingly.
	    taken = ((cb_predictions & 3) >= 2);

	    // Shift out the used-up 2-bit counter, to set up prediction of the next conditional branch.
	    cb_predictions = (cb_predictions >> 2);

	    // (0) Determine if the instruction is a branch.  If it is a branch, determine its type and taken target.
	    bundle[pos].branch = true;
	    bundle[pos].branch_type = BTB_BRANCH;
	    bundle[pos].branch_target = BRANCH_TARGET;

	    // (1) Determine the instruction's next_pc field.
	    bundle[pos].next_pc = (taken ? BRANCH_TARGET : INCREMENT_PC(pc));

	    // (2) Determine if this is the last instruction in the bundle.
	    //     End the fetch bundle at the maximum number of conditional branches, or at call direct, jump indirect, call indirect, or return.
	    if (num_cond_branch == max_cb)
	       terminated = true;

	    break;

         case OP_JAL:
	    // (0) Determine if the instruction is a branch.  ...
	    bundle[pos].branch = true;

	    // (1) Determine the instruction's next_pc field.
	    bundle[pos].next_pc = JUMP_TARGET;

            // According to the ABI, a JAL that saves its return address into X1 is a call instruction.
	    if (insn.rd() == 1) {
	       // CALL DIRECT

	       // (0) ... If it is a branch, determine its type and taken target.
	       bundle[pos].branch_type = BTB_CALL_DIRECT;
	       bundle[pos].branch_target = JUMP_TARGET;

	       // (2) Determine if this is the last instruction in the bundle.
	       //     End the fetch bundle at the maximum number of conditional branches, or at call direct, jump indirect, call indirect, or return.
	       terminated = true;

	       // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
	       update->push_ras = true;
	       update->push_ras_pc = INCREMENT_PC(pc);
	    }
	    else {
	       // JUMP DIRECT
	       // A trace may have any number of jump direct instructions.

	       // (0) ... If it is a branch, determine its type and taken target.
	       bundle[pos].branch_type = BTB_JUMP_DIRECT;
	       bundle[pos].branch_target = JUMP_TARGET;
	    }
            break;

         case OP_JALR:
	    // (0) Determine if the instruction is a branch.  ...
	    bundle[pos].branch = true;

            if ((insn.rd() == 0) && (insn.rs1() == 1)) {
	       // RETURN
	       // According to the ABI, a JALR that discards its return address (writes X0) and jumps to the link register (X1) is a return instruction.

	       // (0) ... If it is a branch, determine its type and taken target.
	       bundle[pos].branch_type = BTB_RETURN;
	       bundle[pos].branch_target = 0;  // invalid for indirect branches

	       // (1) Determine the instruction's next_pc field.
	       bundle[pos].next_pc = ras_predicted_target;

	       // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
	       update->pop_ras = true;
            }
	    else if (insn.rd() == 1) {
	       // CALL INDIRECT
	       // According to the ABI, a JALR that saves its return address into X1 is a call instruction.

	       // (0) ... If it is a branch, determine its type and taken target.
	       bundle[pos].branch_type = BTB_CALL_INDIRECT;
	       bundle[pos].branch_target = 0;  // invalid for indirect branches

	       // (1) Determine the instruction's next_pc field.
	       bundle[pos].next_pc = ib_predicted_target;

	       // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
	       update->push_ras = true;
	       update->push_ras_pc = INCREMENT_PC(pc);
            }
	    else {
	       // JUMP INDIRECT

	       // (0) ... If it is a branch, determine its type and taken target.
	       bundle[pos].branch_type = BTB_JUMP_INDIRECT;
	       bundle[pos].branch_target = 0;  // invalid for indirect branches

	       // (1) Determine the instruction's next_pc field.
	       bundle[pos].next_pc = ib_predicted_target;
	    }

	    // (2) Determine if this is the last instruction in the bundle.
	    //     End the fetch bundle at the maximum number of conditional branches, or at call direct, jump indirect, call indirect, or return.
	    terminated = true;
            break;

         default:
	    // (0) Determine if the instruction is a branch.  ...
	    bundle[pos].branch = false;

	    // (1) Determine the instruction's next_pc field.
	    bundle[pos].next_pc = INCREMENT_PC(pc);
            break;
      }

      // Go to the next instruction in the fetch bundle.
      pc = bundle[pos].next_pc;
      pos++;
   }

   // Finalize the "update" variable (which is needed by the Fetch Unit to speculatively update its predictors and pc).
   // Above, we recorded values for the push_ras/pop_ras related fields.
   // Now, record values for num_cb (number of conditional branches in the fetch bundle) and next_pc (predicted PC of the next fetch bundle).
   assert(pos > 0);	// There must be at least one instruction in the fetch bundle.
   update->next_pc = bundle[pos-1].next_pc;
   update->num_cb = num_cond_branch;

   // Mark any residual slots in the fetch bundle as invalid (no instructions in those slots).
   while (pos < max_length) {
      bundle[pos].valid = false;
      pos++;
   }

   return(true);	// Perfect trace cache always hits.  The fetch bundle has at least one instruction.
}
