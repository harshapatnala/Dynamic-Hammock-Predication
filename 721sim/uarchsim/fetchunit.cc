#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "CacheClass.h"
#include "pipeline.h"
#include <fstream>
#include<stdlib.h>


fetchunit_t::fetchunit_t(uint64_t instr_per_cycle,			// "n"
			 uint64_t cond_branch_per_cycle,		// "m"
			 uint64_t btb_entries,				// total number of entries in the BTB
			 uint64_t btb_assoc,				// set-associativity of the BTB
			 uint64_t cb_pc_length, uint64_t cb_bhr_length,	// gshare cond. br. predictor: pc length (index size), bhr length
			 uint64_t ib_pc_length, uint64_t ib_bhr_length,	// gshare indirect br. predictor: pc length (index size), bhr length
			 uint64_t ras_size,				// # entries in the RAS
			 uint64_t bq_size,				// branch queue size (max. number of outstanding branches)
			 bool tc_enable,				// enable trace cache
			 bool tc_perfect,				// perfect trace cache (only relevant if trace cache is enabled)
			 bool bp_perfect,				// perfect branch prediction
			 bool ic_perfect,				// perfect instruction cache
			 uint64_t ic_sets,				// I$ sets
			 uint64_t ic_assoc,				// I$ set-associativity
			 uint64_t ic_line_size,				// log2(I$ line size in bytes): must be consistent with instr_per_cycle (see ic.h/cc).
			 uint64_t ic_hit_latency,			// I$ hit latency (overridden by Fetch1 stage being 1 cycle deep)
			 uint64_t ic_miss_latency,			// I$ miss latency (overridden by L2$ hit latency, only if L2$ exists)
			 uint64_t ic_num_MHSRs,				// I$ number of MHSRs
			 uint64_t ic_miss_srv_ports,			// see CacheClass.h/cc
			 uint64_t ic_miss_srv_latency,			// see CacheClass.h/cc
			 CacheClass *L2C,				// The L2 cache that backs the instruction cache.
			 mmu_t *mmu,					// mmu is needed by (1) the instruction cache and (2) perfect trace cache mode
			 pipeline_t *proc,				// proc is needed by (1) PAY->map_to_actual() and PAY->predict(), and
									//                   (2) instruction cache's access to proc's stats
			 payload *PAY,			// (1) Payload of fetched instructions. (2) Provides a function that serves as a perfect branch predictor.
	       std::string file //-------------------- ADDED CODE ----------------------------
       ):instr_per_cycle(instr_per_cycle),
	      cond_branch_per_cycle(cond_branch_per_cycle),
	      PAY(PAY),
	      proc(proc),
	      fetch_active(true),
	      pc((uint64_t)0x2000),
	      ic(ic_perfect, mmu, instr_per_cycle,
	         ic_sets, ic_assoc, ic_line_size, ic_hit_latency, ic_miss_latency, ic_num_MHSRs, ic_miss_srv_ports, ic_miss_srv_latency, proc, L2C),
	      ic_miss(false),
	      btb(btb_entries, instr_per_cycle, btb_assoc, cond_branch_per_cycle),
	      tc_enable(tc_enable),
	      tc(tc_perfect, mmu, cond_branch_per_cycle, instr_per_cycle),
	      cb_index(cb_pc_length, cb_bhr_length),
              ib_index(ib_pc_length, ib_bhr_length),
              ras(ras_size),
	      bp_perfect(bp_perfect),
	      bq(bq_size), 
         h_file(file)            { //------------------ ADDED CODE ------------------

   // Memory-allocate the fetch bundle from the instruction cache + BTB or from the trace cache.
   fetch_bundle = new fetch_bundle_t[instr_per_cycle];

   // Memory-allocate the conditional branch (cb) prediction table and indirect branch (ib) prediction table.
   cb = new uint64_t[cb_index.table_size()];
   ib = new uint64_t[ib_index.table_size()];

   for (uint64_t i = 0; i < cb_index.table_size(); i++)
      cb[i] = 0xaaaaaaaa; // Initialize counters to weakly-taken.

   // Memory-allocate FETCH2, the pipeline register between the Fetch1 and Fetch2 stages.
   FETCH2 = new pipeline_register[instr_per_cycle];

   // Initialize the Fetch2 stage's status.
   fetch2_status.valid = false;

   // This assertion is required because BTB bank selection assumes a power-of-two number of BTB banks.
   assert(IsPow2(instr_per_cycle));

   // Initialize measurements.

   meas_branch_n = 0;	// # branches
   meas_jumpdir_n = 0;	// # jumps, direct
   meas_calldir_n = 0;	// # calls, direct
   meas_jumpind_n = 0;	// # jumps, indirect
   meas_callind_n = 0;	// # calls, indirect
   meas_jumpret_n = 0;	// # jumps, return

   meas_branch_m = 0;	// # mispredicted branches
   meas_jumpind_m = 0;	// # mispredicted jumps, indirect
   meas_callind_m = 0;	// # mispredicted calls, indirect
   meas_jumpret_m = 0;	// # mispredicted jumps, return

   meas_jumpind_seq = 0;// # jump-indirect instructions whose targets were the next sequential PC

   meas_btbmiss = 0;	// # of btb misses, i.e., number of discarded fetch bundles (idle fetch cycles) due to a btb miss within the bundle
   
   //----------------------------------------- ADDED CODE --------------------------------
   //hammock_table = new hammock_table_t(h_file);
   btb.construct_hammock_table(h_file);
   fetch_state = REGULAR;
   //--------------------------------------------------------------------------------------
   //branch_count = 0;
   //branch_mispredict_count = 0;
}

fetchunit_t::~fetchunit_t() {
}

void fetchunit_t::spec_update(spec_update_t *update, uint64_t cb_predictions) {
   // Speculatively update the pc with the predicted pc of the next fetch bundle.
   pc = update->next_pc;

   // Speculatively update the BHRs.
   bool taken;
   for (uint64_t i = 0; i < update->num_cb; i++) {
      // The low two bits of cb_predictions correspond to the next two-bit counter to examine (because we shift it right, subsequently).
      // From this two-bit counter, set the taken flag, accordingly.
      taken = ((cb_predictions & 3) >= 2);

      // Shift out the used-up 2-bit counter, to set up the next conditional branch.
      cb_predictions = (cb_predictions >> 2);

      // Update the BHRs of the conditional branch predictor and indirect branch predictor.
      cb_index.update_bhr(taken);
      ib_index.update_bhr(taken);
   }

   // Speculatively update the RAS.
   if (update->pop_ras) {
      assert(!update->push_ras);
      ras.pop();
   }
   if (update->push_ras) {
      assert(!update->pop_ras);
      ras.push(update->push_ras_pc);
   }
}

void fetchunit_t::transfer_fetch_bundle(fetch_state_e fetch_state) {
   uint64_t pos;	// instruction's position in the fetch bundle
   uint64_t index;	// PAY index

   pos = 0;
   while ((pos < instr_per_cycle) && fetch_bundle[pos].valid) {
      //////////////////////////////////////////////////////
      // Put the instruction's payload into PAY.
      //////////////////////////////////////////////////////
      index = PAY->push();
      PAY->buf[index].inst = fetch_bundle[pos].insn;
      PAY->buf[index].pc = fetch_bundle[pos].pc;
      PAY->buf[index].next_pc = fetch_bundle[pos].next_pc;
      PAY->buf[index].branch = fetch_bundle[pos].branch;
      PAY->buf[index].branch_type = fetch_bundle[pos].branch_type;
      PAY->buf[index].branch_target = fetch_bundle[pos].branch_target;
      PAY->buf[index].fflags = 0; // fflags field is always cleaned for newly fetched instructions
      //--------------- ADDED CODE ------------------
      PAY->buf[index].instruction_type = fetch_bundle[pos].region_type;
      PAY->buf[index].is_hammock = fetch_bundle[pos].is_hammock;
      if(PAY->buf[index].instruction_type == CMOV) PAY->buf[index].CMOV_log_reg = fetch_bundle[pos].cmov_log_reg;
      //--------------------------------------------

      // Clear the trap storage before the first time it is used.
      PAY->buf[index].trap.clear();
      assert(!PAY->buf[index].trap.valid());

      // Check if there was an fetch exception.
      if (fetch_bundle[pos].exception) {
         if (fetch_bundle[pos].exception_cause == CAUSE_MISALIGNED_FETCH) {
            PAY->buf[index].trap.post(trap_instruction_address_misaligned(fetch_bundle[pos].pc));
         } else if (fetch_bundle[pos].exception_cause == CAUSE_FAULT_FETCH) {
            PAY->buf[index].trap.post(trap_instruction_access_fault(fetch_bundle[pos].pc));
         } else {
            assert(0);
         }
      }

      //////////////////////////////////////////////////////
      // map_to_actual()
      //////////////////////////////////////////////////////

      // Try to link the instruction to the corresponding instruction in the functional simulator.
      // NOTE: Even when NOPs are injected, successfully mapping to actual is not a problem,
      // as the NOP instructions will never be committed.
      PAY->map_to_actual(proc, index);

      //////////////////////////////////////////////////////
      // Put the PAY index into the FETCH2 pipeline register.
      //////////////////////////////////////////////////////
      assert(!FETCH2[pos].valid);
      FETCH2[pos].valid = true;
      FETCH2[pos].index = index;

      // Go to the next instruction.
      pos++;
   }

   // Assert that the fetch bundle has at least one instruction.
   assert(FETCH2[0].valid);
}

void fetchunit_t::squash_fetch2() {
   fetch2_status.valid = false;
   for (uint64_t i = 0; i < instr_per_cycle; i++)
      FETCH2[i].valid = false;
   
   fetch_state = REGULAR;
   btb.reset_state();

}

// Fetch1 pipeline stage.
void fetchunit_t::fetch1(cycle_t cycle) {
   // Stall if any of the following conditions hold:
   // 1. The Fetch2 bundle hasn't advanced.
   // 2. Instruction fetching is disabled until a serializing instruction (fetch exception, amo, or csr instruction) retires.
   // 3. The Fetch1 stage is waiting for an instruction cache miss to resolve.
   if (fetch2_status.valid || !fetch_active || (ic_miss && (cycle < ic_miss_resolve_cycle)))
      return;

   // If we *were* waiting for an instruction cache miss to resolve, we are no longer waiting.
   ic_miss = false;

   ///////////////////////////////////////////////////////////////////////////////////////////////////////////
   // Search all the structures "in parallel".
   // - real branch predictor or perfect branch predictor (including conditional branch predictor,
   //   indirect branch predictor, and RAS)
   // - trace cache
   // - instruction cache + BTB
   ///////////////////////////////////////////////////////////////////////////////////////////////////////////

   uint64_t cb_predictions;			  // "m" conditional branch predictions packed into a uint64_t
   uint64_t ib_predicted_target;		  // predicted target from the indirect branch predictor
   uint64_t ras_predicted_target;		  // predicted target from the return address stack (only popped if fetch bundle ends in a return)

   // This guides speculatively updating the pc, BHRs, and RAS, after accessing the trace cache and instruction cache + BTB.
   spec_update_t update;

   // Access the branch predictor.
   if (bp_perfect) {
      // Perfect branch predictor.
      uint64_t indirect_target;
      PAY->predict(proc, pc, instr_per_cycle, cb_predictions, indirect_target);
      ib_predicted_target = indirect_target;
      ras_predicted_target = indirect_target;
   }
   else {
      // Real branch predictor.

      // Get "m" predictions from the conditional branch predictor.
      // "m" two-bit counters are packed into a uint64_t.
      cb_predictions = cb[cb_index.index(pc)];

      // Get a predicted target from the indirect branch predictor.  It is only used if the fetch bundle ends at a jump indirect or call indirect.
      ib_predicted_target = ib[ib_index.index(pc)];

      // Get a predicted target from the return address stack.  This is only a peek: it is popped only if ultimately used.
      ras_predicted_target = ras.peek();
   }

   // Access the trace cache.
   bool tc_hit = false;
   if (tc_enable)
      tc_hit = tc.lookup(pc, cb_predictions, ib_predicted_target, ras_predicted_target, fetch_bundle, &update);

   // Access the instruction cache + BTB.

   // There are several simulator reasons for gating the instruction cache + BTB search, with trace cache hit:
   // 1. We only need one of each of the "fetch_bundle" and "update" variables.
   //    If the trace cache hit, it loaded fetch_bundle and recorded update, and we don't want the instruction cache + BTB to clobber them.
   // 2. Faster simulation.
   // In hardware, for performance, the trace cache and instruction cache + BTB would be accessed in parallel, and conceptually we are still doing that.
   // (Note: One possible inaccuracy here, is that by gating, we don't train the instruction cache + BTB for would-be misses when the trace cache hits.
   //  On the other hand, this is a possible policy anyway, e.g., to steer instructions that hit in the trace cache away from the instruction cache + BTB.)
   if (!tc_hit) {
      // We must call ic.lookup() before btb.lookup(), so that btb.lookup() can terminate the fetch bundle at an exception if there is one.
      // For each instruction in the fetch_bundle, the I$ sets: exception, exception_cause, and insn.
      // For each instruction in the fetch_bundle, the BTB sets: valid, btb_hit, pc, next_pc.
      
      //Access the I Cache ,if not in CMOV state.
     //HP-- printf("Current State in Fetch 1 - %d\n", fetch_state);
      if(fetch_state == REGULAR || fetch_state == PREDICATED_REGION) { 
         ic_miss = !(ic.lookup(cycle, pc, fetch_bundle, ic_miss_resolve_cycle));

      // There are several simulator reasons for gating the BTB search, with instruction cache hit:
      // 1. btb.lookup() uses exception bits written by ic.lookup() into the bundle[], to early-terminate the fetch bundle.
      //    These bits are uninitialized in the case of an instruction cache miss.  It would be ok to call btb.lookup(), just random and wasteful.
      // 2. Faster simulation.
      // Nevertheless, conceptually, the instruction cache and BTB are searched in parallel.

         if (!ic_miss) {
            btb.lookup(pc, cb_predictions, ib_predicted_target, ras_predicted_target, fetch_bundle, &update);
            fetch_state = update.state;
         }
      }
      //Bypass Icache access and process/create the CMOV bundle in BTB lookup
      else if(fetch_state == CMOV_Region) {
         btb.lookup(pc, cb_predictions, ib_predicted_target, ras_predicted_target, fetch_bundle, &update);
         fetch_state = update.state;
      }
   }

   if (tc_hit || !ic_miss) {
      ///////////////////////////////////////////////////////////////////////////////////////////////////////////
      // Save the fetch bundle's pc, BHRs (prior to the fetch bundle), RAS TOS (prior to the fetch bundle),
      // and PAY's current position, in the fetch2_status register.
      // We checkpoint this state so that we can squash the fetch bundle and repredict it, if the Fetch2 stage
      // detects a "misfetch".
      // A misfetch is when the fetch bundle came from the instruction cache + BTB, but the BTB hits were flawed.
      ///////////////////////////////////////////////////////////////////////////////////////////////////////////

      fetch2_status.valid = true;
      fetch2_status.pc = pc;
      fetch2_status.cb_bhr = cb_index.get_bhr();
      fetch2_status.ib_bhr = ib_index.get_bhr();
      fetch2_status.ras_tos = ras.get_tos();
      fetch2_status.pay_checkpoint = PAY->checkpoint();
      fetch2_status.tc_hit = tc_hit;

      ///////////////////////////////////////////////////////////////////////////////////////////////////////////
      // Transfer the fetch bundle to PAY->buf[] and push PAY indices into the FETCH2 pipeline register.
      ///////////////////////////////////////////////////////////////////////////////////////////////////////////
      transfer_fetch_bundle(fetch_state);

      ///////////////////////////////////////////////////////////////////////////////////////////////////////////
      // Speculatively update the pc, BHRs, and RAS.
      ///////////////////////////////////////////////////////////////////////////////////////////////////////////
      spec_update(&update, cb_predictions);
   }
}

// Fetch2 pipeline stage.
// If it returns true: call fetchunit_t::fetch1() after.
// If it returns false: do NOT call fetchunit_t::fetch1() after, because of a misfetch recovery.
bool fetchunit_t::fetch2(pipeline_register DECODE[]) {
   uint64_t pos;	// position of instruction in the fetch bundle
   uint64_t index;	// PAY index
   bool exception;	// if true, an exception was found in the fetch bundle
   bool serialize;	// if true, a serializing instruction was found in the fetch bundle
   bool misfetch;	// if true, the instruction cache supplied a misfetched bundle (details below)
   bool is_branch_insn;

   // Do nothing if there isn't a fetch bundle in the Fetch2 stage.
   if (!fetch2_status.valid) {
      assert(!FETCH2[0].valid);
      return(true);
   }

   //////////////////////////////////////////////////////////////////////////////////
   // Step 1: Predecode the fetch bundle to identify the following conditions.
   // - exception: Assert no valid instructions follow, and make the Fetch1 stage idle.
   // - serializing instructions (amo, csr): Invalidate following instructions,
   //   and make the Fetch1 stage idle.
   // - "misfetched" bundle: See discussion below regarding BTB-missed branches and BTB-mis-identified non-branches.
   //////////////////////////////////////////////////////////////////////////////////

   // "misfetched" bundle discussion:
   //
   // If the fetch bundle came from the instruction cache, check if the BTB missed any branches or mis-identified non-branches in the fetch bundle.
   // If the BTB generated wrong branch information, the fetch bundle is potentially malformed with respect to the "intent"
   // of the multiple-branch predictor.
   // That is, it's possible that one or more branch predictions were not lined up with their intended branches.
   // For a misfetched bundle:
   // a. Train the BTB for the missing branches and/or invalidate the non-branches.
   // b. Squash the misfetched bundle in the Fetch2 stage.
   // c. Rollback the Fetch1 stage to what its state was just prior to the misfetched bundle -- in order to repredict it.
   // d. Return "false" from this function, to signal to the caller that it should NOT call fetchunit_t::fetch1()
   //    after this call to fetchunit_t::fetch2().  Rather, it should wait until the next cycle.

   pos = 0;
   exception = false;
   serialize = false;
   misfetch = false;
   while ((pos < instr_per_cycle) && FETCH2[pos].valid) {
      // If we had set exception or serialize in the previous iteration,
      // the instruction for this iteration should be invalid and we should have exited the loop.
      assert(!exception);
      assert(!serialize);

      // get PAY index
      index = FETCH2[pos].index;

      if (PAY->buf[index].trap.valid()) {
         // The instruction triggered an exception during its fetch stage, therefore has a valid trap information.
         exception = true;

	 // If the offending instruction is not in the last possible slot,
	 // assert that the next slot is invalid.
         if ((pos + 1) < instr_per_cycle)
	    assert(!FETCH2[pos + 1].valid);
      
         // Make the Fetch1 stage idle.  It will become active again when the offending instruction reaches head of Active List.
	 fetch_active = false;
      }

      switch (PAY->buf[index].inst.opcode()) {
         case OP_AMO:
         case OP_SYSTEM:
	 //case OP_MISC_MEM:  // FIX_ME: Make fence a serializing instruction, add fence flag to Active List, and reference this flag to full squash at Retire stage.
	    serialize = true;

	    // If the serializing instruction is not in the last possible slot,
	    // make the next slot is invalid.
            if ((pos + 1) < instr_per_cycle)
	       FETCH2[pos + 1].valid = false;

            // Make the Fetch1 stage idle.  It will become active again when the serializing instruction reaches head of Active List.
	    fetch_active = false;

	    is_branch_insn = false;
	    break;

         case OP_JAL:
         case OP_JALR:
         case OP_BRANCH:
	    is_branch_insn = true;
	    break;

         default:
	    is_branch_insn = false;
	    break;
      }

      // Check the branch information from the BTB or TC.
      if (fetch2_status.tc_hit) {
         // The fetch bundle came from the trace cache.
         // The trace cache should always be correct in identifying branches in the fetch bundle.
         assert(PAY->buf[index].branch == is_branch_insn);
      }
      else {
         // The fetch bundle came from the BTB + I$.
         if (is_branch_insn) {
            //printf("CHECKING PC in FETCH2 Decoding TYPE OPCODE- %llx %d %dn", PAY->buf[index].pc, PAY->buf[index].branch_type, PAY->buf[index].inst.opcode());
            // Actually a branch instruction.
            //BYPASS THIS IF BRANCH TYPE WAS HAMMOCK OR IN THE THEN CLAUSE, WE HAVE AN UNCONDITIONAL JUMP. AS WE DON'T NEED TO HAVE THIS BRANCH/JUMP IN BTB.
            if(!(PAY->buf[index].is_hammock) && !((PAY->buf[index].inst.opcode() == OP_JAL) && (PAY->buf[index].instruction_type == THEN))) {
               btb_branch_type_e real_branch_type;
               uint64_t real_target;
               real_branch_type = btb_t::decode(PAY->buf[index].inst, PAY->buf[index].pc, real_target);
      
               if (!PAY->buf[index].branch ||
                  (PAY->buf[index].branch_type != real_branch_type) ||
                  ((PAY->buf[index].inst.opcode() != OP_JALR) && (PAY->buf[index].branch_target != real_target))) {
                  // The fetch bundle came from the instruction cache and this branch was missed by the BTB -OR- the branch type and/or target was mispredicted.
                  // The type and/or target may change due to either of two reasons:
                  // 1. Self-modifying code (unlikely in SPEC).
                  // 2. The BTB is caching a non-instruction (data) corresponding to a "PC" that is not in the text segment, and the data changed.
                  //    Note, the BTB is trained speculatively. If it is trained with bundles down the wrong path of a branch (particularly indirect branches), it can be trained
                  //    with a "PC" that falls somewhere outside the text segment. Our proxy kernel marks heap pages as RWX, so no fetch exception is triggered.
                  // Note: The branch_target from the BTB is not valid for indirect branches (OP_JALR); therefore, gate the branch_target check for indirect branches.
                  misfetch = true;
                  //printf("IN FETCH2 MISFETCH LOGIC, PC-%llx\n", PAY->buf[index].pc);
                  // Train the BTB for this branch.
                  // The update interface takes in the start PC of the fetch bundle and the position of the branch within it.
                  btb.update(fetch2_status.pc, pos, PAY->buf[index].inst);
         
                  // Update BTB misses.
                  // Note: This is a "speculative" measurement, i.e, it includes BTB misses of instructions down the wrong path after mispredicted branches.
                  // It's hard to measure btb misses for only correct-path instructions because they aren't posted in the branch queue (due to misfetch).
                  meas_btbmiss++;
               }
            }
         } else {
            // Actually NOT a branch instruction.
            if (PAY->buf[index].branch) {
               // A non-branch was misidentified by the BTB as a branch.
               // This occasionally happens if the memory content at the "PC" previously encoded a branch (and the BTB was trained for that),
               // but it changed and now no longer encodes a branch.
               // It is possible for the same reasons why a branch's type and/or target could be misidentified.
               misfetch = true;
               
               // The pipeline should invalidate the incorrect BTB entry.
               btb.invalidate(fetch2_status.pc, pos);
               
               // Update BTB misses
               meas_btbmiss++;
            }
         }
      }

      pos++;
   }

   if (misfetch) {
      // It's possible to have a misfetched bundle containing an exception or serializing instruction.
      // This can happen in the loop, above, if the exception or serializing instruction is after the
      // first BTB-missed branch.
      //
      // If so, set fetch_active back to true.
      fetch_active = true;

      // a. Train the BTB for the missing branches.
      // DONE in the loop above.

      // b. Squash the misfetched bundle in the Fetch2 stage.
      squash_fetch2();

      // c. Rollback the Fetch1 stage to what its state was just prior to the misfetched bundle -- in order to repredict it.
      pc = fetch2_status.pc;
      cb_index.set_bhr(fetch2_status.cb_bhr);
      ib_index.set_bhr(fetch2_status.ib_bhr);
      ras.set_tos(fetch2_status.ras_tos);
      PAY->restore(fetch2_status.pay_checkpoint);

      // d. Return "false" from this function, to signal to the caller that it should NOT call fetchunit_t::fetch1()
      //    after this call to fetchunit_t::fetch2().  Rather, it should wait until the next cycle.
      return(false);
   }

   //////////////////////////////////////////////////////////////////////////////////
   // Step 2:
   //
   // If the decode bundle in the Decode stage has not advanced,
   // stall by returning true immediately.
   //
   // On the other hand, if the decode bundle in the Decode stage has advanced:
   // a. Transfer the fetch bundle from FETCH2 to DECODE.
   // b. Push branches onto the branch queue.
   // c. Reset fetch2_status.
   // d. Return true.
   //////////////////////////////////////////////////////////////////////////////////

   // Stall if the decode bundle in the Decode stage hasn't advanced.
   if (DECODE[0].valid)
      return(true);

   // a. Transfer the fetch bundle from FETCH2 to DECODE.
   // b. Push branches onto the branch queue.

   bool taken;		// taken/not-taken prediction for the current conditional branch (where we are at in the fetch bundle)
   uint64_t pred_tag;	// pred_tag is the index into the branch queue for the newly pushed branch
   bool pred_tag_phase;	// this will get appended to pred_tag so that the user interacts with the Fetch Unit via a single number
   uint64_t fetch_cb_pos_in_entry = 0; // Identifies this conditional branch's position within the conditional branch prediction bundle.

   // Recreate a precise BHR at each branch queue entry, starting with the fetch2_status' BHR that is just prior to the fetch bundle.
   uint64_t my_cb_bhr = fetch2_status.cb_bhr;
   uint64_t my_ib_bhr = fetch2_status.ib_bhr;

   pos = 0;
   while ((pos < instr_per_cycle) && FETCH2[pos].valid) {
      assert(!DECODE[pos].valid);
      DECODE[pos].valid = true;
      DECODE[pos].index = FETCH2[pos].index;
      FETCH2[pos].valid = false;

      // get PAY index
      index = FETCH2[pos].index;
      //printf("IN FETCH2 PC %llx\t", PAY->buf[index].pc);
      if (PAY->buf[index].branch && PAY->buf[index].branch_type != HAMMOCK) {
      // Push an entry into the branch queue.
      // This merely allocates the entry; below, we set the entry's contents.
      //printf("Branch Queue PC %llx\n", PAY->buf[index].pc);
      bq.push(pred_tag, pred_tag_phase);
      //if(PAY->buf[index].pc == 0x1a5a0) branch_count++;

      // Merge the pred_tag and pred_tag_phase into a single pred_tag, and assign it to the instruction.
      PAY->buf[index].pred_tag = ((pred_tag << 1) | (pred_tag_phase ? 1 : 0));

      // Set up context-related fields in the new branch queue entry.
      bq.bq[pred_tag].branch_type = PAY->buf[index].branch_type;
      bq.bq[pred_tag].precise_cb_bhr = my_cb_bhr;
      bq.bq[pred_tag].precise_ib_bhr = my_ib_bhr;
      bq.bq[pred_tag].precise_ras_tos = fetch2_status.ras_tos;  // FIX_ME: unsure about this, if bundle ends in a return.
      bq.bq[pred_tag].fetch_pc = fetch2_status.pc;
      bq.bq[pred_tag].fetch_cb_bhr = fetch2_status.cb_bhr;
      bq.bq[pred_tag].fetch_ib_bhr = fetch2_status.ib_bhr;
      bq.bq[pred_tag].fetch_cb_pos_in_entry = 0;   // Only relevant for conditional branches, so it may be other than 0 for them.

      // Initialize the misp. flag to indicate, as far as we know at this point, the branch is not mispredicted.
      bq.bq[pred_tag].misp = false;

      // Record the prediction.
      taken = (PAY->buf[index].next_pc != INCREMENT_PC(PAY->buf[index].pc));
      bq.bq[pred_tag].taken = taken;
      bq.bq[pred_tag].next_pc = PAY->buf[index].next_pc;

      // If this is a conditional branch:
      // - Record its position within the conditional branch prediction bundle (fetch_cb_pos_in_entry).
      // - Update the precise BHRs.
      if (PAY->buf[index].branch_type == BTB_BRANCH) {
         // Record this conditional branch's position within the conditional branch prediction bundle.
         bq.bq[pred_tag].fetch_cb_pos_in_entry = fetch_cb_pos_in_entry;

         // Increment the position to set up for the next conditional branch in the conditional branch prediction bundle.
         fetch_cb_pos_in_entry++;

         // Update "my" BHRs of the conditional branch predictor and indirect branch predictor.
         // This does NOT affect the predictors' BHRs, which were already speculatively updated in the Fetch1 stage.
         my_cb_bhr = cb_index.update_my_bhr(my_cb_bhr, taken);
         my_ib_bhr = ib_index.update_my_bhr(my_ib_bhr, taken);
            }
      }

      // Go to next instruction in the fetch bundle.
      pos++;
   }
   //printf("\n");
   // c. Reset fetch2_status.
   fetch2_status.valid = false;

   // d. Return true.
   return(true);
}

// A mispredicted branch was detected.
// 1. Roll-back the branch queue to the mispredicted branch's entry.
// 2. Correct the mispredicted branch's information in its branch queue entry.
// 3. Restore checkpointed global histories and the RAS (as best we can for RAS).
// 4. Note that the branch was mispredicted (for measuring mispredictions at retirement).
// 5. Restore the pc.
// 6. Go active again, whether or not currently active (restore fetch_active).
// 7. Squash the fetch2_status register and FETCH2 pipeline register.
void fetchunit_t::mispredict(uint64_t branch_pred_tag, bool taken, uint64_t next_pc) {
   // Extract the pred_tag and pred_tag_phase from the unified branch_pred_tag.
   //if(temp_pc == 0x1a5a0) branch_mispredict_count++;
   uint64_t pred_tag = (branch_pred_tag >> 1);
   bool pred_tag_phase = (((branch_pred_tag & 1) == 1) ? true : false);

   // 1. Roll-back the branch queue to the mispredicted branch's entry.
   //    Then push the branch back onto it.

   bq.rollback(pred_tag, pred_tag_phase, true);

   uint64_t temp_pred_tag;
   bool temp_pred_tag_phase;
   bq.push(temp_pred_tag, temp_pred_tag_phase);	// Need to push the branch back onto the branch queue.
   assert((temp_pred_tag == pred_tag) && (temp_pred_tag_phase == pred_tag_phase));

   // 2. Correct the mispredicted branch's information in its branch queue entry.

   assert(bq.bq[pred_tag].next_pc != next_pc);
   bq.bq[pred_tag].next_pc = next_pc;

   if (bq.bq[pred_tag].branch_type == BTB_BRANCH)
      assert(bq.bq[pred_tag].taken != taken);

   bq.bq[pred_tag].taken = taken;
 
   // 3. Restore checkpointed global histories and the RAS (as best we can for RAS).

   cb_index.set_bhr(bq.bq[pred_tag].precise_cb_bhr);
   ib_index.set_bhr(bq.bq[pred_tag].precise_ib_bhr);
   ras.set_tos(bq.bq[pred_tag].precise_ras_tos);

   // If the resolved branch is a conditional branch, don't forget to include its corrected outcome
   // in the BHRs that will kick off predictions after this resolved branch.
   if (bq.bq[pred_tag].branch_type == BTB_BRANCH) {
      cb_index.update_bhr(taken);
      ib_index.update_bhr(taken);
   }

   // 4. Note that the branch was mispredicted (for measuring mispredictions at retirement).

   bq.bq[pred_tag].misp = true;

   // 5. Restore the pc.

   pc = next_pc;

   // 6. Go active again, whether or not currently active (restore fetch_active).

   fetch_active = true;

   // 7. Squash the fetch2_status register and FETCH2 pipeline register.

   squash_fetch2();
}


// Commit the indicated branch from the branch queue.
// We assert that it is at the head.
void fetchunit_t::commit(uint64_t branch_pred_tag) {
   // Pop the branch queue. It returns the pred_tag/pred_tag_phase of the head entry prior to popping it.
   uint64_t pred_tag;
   bool pred_tag_phase;
   bq.pop(pred_tag, pred_tag_phase);

   // Assert that the branch_pred_tag (pred_tag of the branch being committed from the pipeline) corresponds to the popped branch queue entry.
   assert(branch_pred_tag == ((pred_tag << 1) | (pred_tag_phase ? 1 : 0)));

   // Update the conditional branch predictor or indirect branch predictor.
   // Update measurements.
   uint64_t *cb_counters;	// FYI: The compiler forbids declaring these four local variables inside "case BTB_BRANCH:".
   uint64_t shamt;
   uint64_t mask;
   uint64_t ctr;
   switch (bq.bq[pred_tag].branch_type) {
      case BTB_BRANCH:
	 // Re-reference the conditional branch predictor, using the same context that was used by
	 // the fetch bundle that this branch was a part of.
         // Using this original context, we re-reference the same "m" counters from the conditional branch predictor.
         // "m" two-bit counters are packed into a uint64_t.
	 cb_counters = &(  cb[ cb_index.index(bq.bq[pred_tag].fetch_pc, bq.bq[pred_tag].fetch_cb_bhr) ]  );

	 // Prepare for reading and writing the 2-bit counter that was used to predict this branch.
	 // We need a shift-amount ("shamt") and a mask ("mask") that can be used to read/write just that counter.
	 // "shamt" = the branch's position in the entry times 2, for 2-bit counters.
	 // "mask" = (3 << shamt).
	 shamt = (bq.bq[pred_tag].fetch_cb_pos_in_entry << 1);
	 mask = (3 << shamt);

	 // Extract a local copy of the 2-bit counter that was used to predict this branch.
	 ctr = (((*cb_counters) & mask) >> shamt);

	 // Increment or decrement the local copy of the 2-bit counter, based on the branch's outcome.
	 if (bq.bq[pred_tag].taken) {
	    if (ctr < 3)
	       ctr++;
	 }
	 else {
	    if (ctr > 0)
	       ctr--;
	 }

	 // Write the modified local copy of the 2-bit counter back into the predictor's entry.
	 *cb_counters = (((*cb_counters) & (~mask)) | (ctr << shamt));

	 // Update measurements.
	 meas_branch_n++;
	 if (bq.bq[pred_tag].misp)
	    meas_branch_m++;
         break;

      case BTB_JUMP_DIRECT:
	 // Update measurements.
	 meas_jumpdir_n++;
	 assert(!bq.bq[pred_tag].misp);
         break;

      case BTB_CALL_DIRECT:
	 // Update measurements.
	 meas_calldir_n++;
	 assert(!bq.bq[pred_tag].misp);
         break;

      case BTB_JUMP_INDIRECT:
      case BTB_CALL_INDIRECT:
	 // Re-reference the indirect branch predictor, using the same context that was used by
	 // the fetch bundle that this branch was a part of.
	 ib[ ib_index.index(bq.bq[pred_tag].fetch_pc, bq.bq[pred_tag].fetch_ib_bhr) ] = bq.bq[pred_tag].next_pc;

	 // Update measurements.
	 if (bq.bq[pred_tag].branch_type == BTB_JUMP_INDIRECT) {
	    meas_jumpind_n++;
	    if (bq.bq[pred_tag].misp)
	       meas_jumpind_m++;

            // Check for, and count, an unconditional jump to the next sequential pc (can happen for first case of a switch() statement).
	    if (!bq.bq[pred_tag].taken)
	       meas_jumpind_seq++;
	 }
	 else {
	    meas_callind_n++;
	    if (bq.bq[pred_tag].misp)
	       meas_callind_m++;
	 }
         break;

      case BTB_RETURN:
         // Update measurements.
	 meas_jumpret_n++;
	 if (bq.bq[pred_tag].misp)
	    meas_jumpret_m++;
         break;

      default:
	 assert(0);
         break;
   }
}


// Complete squash.
// 1. Roll-back the branch queue to the head entry.
// 2. Restore checkpointed global histories and the RAS (as best we can for RAS).
// 3. Restore the pc.
// 4. Go active again, whether or not currently active (restore fetch_active).
// 5. Squash the fetch2_status register and FETCH2 pipeline register.
// 6. Reset ic_miss (discard pending I$ misses).
void fetchunit_t::flush(uint64_t pc) {
   uint64_t pred_tag;

   // 1. Roll-back the branch queue to the head entry.
   pred_tag = bq.flush();  // "pred_tag" is the index of the head entry.

   // 2. Restore checkpointed global histories and the RAS (as best we can for RAS).
   cb_index.set_bhr(bq.bq[pred_tag].precise_cb_bhr);
   ib_index.set_bhr(bq.bq[pred_tag].precise_ib_bhr);
   ras.set_tos(bq.bq[pred_tag].precise_ras_tos);

   // 3. Restore the pc.
   this->pc = pc;

   // 4. Go active again, whether or not currently active (restore fetch_active).
   fetch_active = true;

   // 5. Squash the fetch2_status register and FETCH2 pipeline register.
   squash_fetch2();

   // 6. Reset ic_miss (discard pending I$ misses).
   ic_miss = false;
}


// Output all branch prediction measurements.

#define BP_OUTPUT(fp, str, n, m, i) \
	fprintf((fp), "%s%10lu %10lu %5.2lf%% %5.2lf\n", (str), (n), (m), 100.0*((double)(m)/(double)(n)), 1000.0*((double)(m)/(double)(i)))

void fetchunit_t::output(uint64_t num_instr, uint64_t num_cycles, FILE *fp) {
   uint64_t all = (meas_branch_n + meas_jumpdir_n + meas_calldir_n + meas_jumpind_n + meas_callind_n + meas_jumpret_n);
   uint64_t all_misp = (meas_branch_m + meas_jumpind_m + meas_callind_m + meas_jumpret_m);
   fprintf(fp, "BRANCH PREDICTION MEASUREMENTS---------------------\n");
   fprintf(fp, "Type                      n          m     mr  mpki\n");
   BP_OUTPUT(fp, "All              ", all, all_misp, num_instr);
   BP_OUTPUT(fp, "Branch           ", meas_branch_n, meas_branch_m, num_instr);
   BP_OUTPUT(fp, "Jump Direct      ", meas_jumpdir_n, (uint64_t)0, num_instr);
   BP_OUTPUT(fp, "Call Direct      ", meas_calldir_n, (uint64_t)0, num_instr);
   BP_OUTPUT(fp, "Jump Indirect    ", meas_jumpind_n, meas_jumpind_m, num_instr);
   BP_OUTPUT(fp, "Call Indirect    ", meas_callind_n, meas_callind_m, num_instr);
   BP_OUTPUT(fp, "Return           ", meas_jumpret_n, meas_jumpret_m, num_instr);
   fprintf(fp, "(Number of Jump Indirects whose target was the next sequential PC = %lu)\n", meas_jumpind_seq);
   fprintf(fp, "BTB MEASUREMENTS-----------------------------------\n");
   fprintf(fp, "BTB misses (fetch cycles squashed due to a BTB miss) = %lu (%.2f%% of all cycles)\n", meas_btbmiss, 100.0*((double)meas_btbmiss/(double)num_cycles));
   //fprintf(fp, "Number of Times Branch Hammock fetched- %lu\n", branch_count);
   //fprintf(fp, "Number of Times Branch Hammock Mispredicted- %lu\n", branch_mispredict_count);
   //fprintf(fp, "Misprediction Rate for Hammock- %0.3f\n", (float)branch_mispredict_count/branch_count);
}

void fetchunit_t::setPC(uint64_t pc) {
   this->pc = pc;
}

uint64_t fetchunit_t::getPC() {
   return(pc);
}

bool fetchunit_t::active() {
   return(fetch_active);
}
