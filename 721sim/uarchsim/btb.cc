#include <cinttypes>
#include <cassert>
#include <cmath>

#include "processor.h"
#include "decode.h"
#include "config.h"

#include "fetchunit_types.h"
#include "btb.h"


hammock_table_t::hammock_table_t(std::string some_file) {
   hammock_file = some_file;
   if(hammock_file == "") fprintf(stderr, "NOTE: DHP Info File Not Specified, Starting Simulation without Predication Support\n");
   else {
      file.open(hammock_file);
      if(!(file.is_open())) {
         fprintf(stderr, "ERROR:Can't Open Hammock Info File");
         exit(EXIT_FAILURE);
      }
      else {
         fprintf(stderr, "---------------- DHP Project -------------\n-- Succesfully Opened Hammock Info File --\n");
      }

      uint64_t temp_pc;
      hammock_entry entry;
      while(file >> std::hex >> temp_pc >> entry.RPC >> std::dec >> entry.then_length >> entry.else_valid >> entry.num_cmovs) {
         entry.CMOV = new uint64_t[entry.num_cmovs];
         for(uint64_t i=0; i < entry.num_cmovs; i++) {
            if(!(file >> entry.CMOV[i])) {
               fprintf(stderr, "ERROR: Can't Read CMOV Info from File\n");
               exit(EXIT_FAILURE);
            }
         }
         if(table.find(temp_pc) == table.end()) {
            table.insert(std::make_pair(temp_pc, entry));
         }
      }  
   }
}

//Searches the table if the PC is a candidate for hammock. Find found, returns true
bool hammock_table_t::search_hammock(uint64_t pc, hammock_entry& entry) {
   auto it = table.find(pc);
   if(it != table.end()) {
      entry = it->second;
      return true;
   }
   else return false;
}



btb_t::btb_t(uint64_t num_entries, uint64_t banks, uint64_t assoc, uint64_t cond_branch_per_cycle) {
   this->banks = banks;
   this->sets = (num_entries/(banks*assoc));
   this->assoc = assoc;
   this->cond_branch_per_cycle = cond_branch_per_cycle;

   assert(IsPow2(banks));
   assert(IsPow2(sets));

   log2banks = (uint64_t) log2((double)banks);
   log2sets = (uint64_t) log2((double)sets);

   // Allocate the 3D array.
   btb = new btb_entry_t **[banks];
   for (uint64_t b = 0; b < banks; b++) {
      btb[b] = new btb_entry_t *[sets];
      for (uint64_t s = 0; s < sets; s++) {
         btb[b][s] = new btb_entry_t[assoc];
	 for (uint64_t way = 0; way < assoc; way++) {
	    btb[b][s][way].valid = false;
	    btb[b][s][way].lru = way;
	 }
      }
   }
   state = REGULAR;
   then_count = 0;
}


btb_t::~btb_t() {
}

void btb_t::construct_hammock_table(std::string file) {
   hammock_table = new hammock_table_t(file);
}

void btb_t::create_cmovs(hammock_entry entry) {

   //CHANGE ME. ASK
   for(uint64_t i=0; i < entry.num_cmovs; i++) {
      cmov_instructions.push_back(entry.CMOV[i]);
   }
}

void btb_t::reset_state() {
   state = REGULAR;
   then_count = 0;
   cmov_instructions.clear();
}
//
// Inputs:
// pc: The start pc of the fetch bundle.
// cb_predictions: A uint64_t packed with "m" 2-bit counters for predicting conditional branches.
// ib_predicted_target: Predicted target for a jump indirect or call indirect, if the fetch bundle ends with this type of branch instruction.
// ras_predicted_target: Predicted target for a return, if the fetch bundle ends with this type of branch instruction.
//
// this->banks: "n", the number of BTB banks, which the Fetch Unit had set equal to the maximum-length sequential fetch bundle.
// this->cond_branch_per_cycle: "m", the maximum number of conditional branches allowed in a fetch bundle.
//
// Outputs:
// 1. bundle[]: The predicted sequential fetch bundle.
//    For each instruction slot in the fetch bundle, the BTB sets six fields (valid, branch, branch_type, branch_target, pc, next_pc) and
//    the I$ sets three fields (exception, exception_cause, and insn).  This function, btb_t::lookup(), performs the BTB part.
// 2. *update: This is a data structure needed by the Fetch Unit to speculatively update its predictors (BHRs, RAS) and pc.
//    In particular, the Fetch Unit needs to know:
//    - The predicted PC of the next fetch bundle, to update its pc.
//    - How many conditional branches are in the assembled fetch bundle, to know how many predictions to shift into its BHRs.
//    - Whether or not it needs to pop the RAS.
//    - Whether or not it needs to push the RAS, and, if so, which pc to push onto the RAS.
void btb_t::lookup(uint64_t pc, uint64_t cb_predictions, uint64_t ib_predicted_target, uint64_t ras_predicted_target, fetch_bundle_t bundle[], spec_update_t *update) {
   uint64_t btb_bank;
   uint64_t btb_pc;
   uint64_t set;
   uint64_t way;
   bool taken;
   uint64_t num_cond_branch = 0;
   bool terminated = false;
   uint64_t pos = 0;

   // hammock_entry entry;

   // Initialize these two fields in the "update" variable (which is needed by the Fetch Unit to speculatively update its predictors and pc).
   // Initially assume the fetch bundle doesn't end in a call (push_ras) or return (pop_ras) instruction, and set to true if and when we determine that it does.
   update->pop_ras = false;
   update->push_ras = false;
   //printf("Current State - %d\n", state);
   while ((pos < banks) && !terminated) {	// "pos" is position of the instruction within the maximum-length sequential fetch bundle.
      // This instruction in the bundle is valid.
      bundle[pos].valid = true;

      // Each instruction in the bundle carries with it, its full pc.
      bundle[pos].pc = (pc + (pos << 2));
      //printf("PC - %llx\n", bundle[pos].pc);
     //HP-- printf("Current PC in BTB Lookup - %x State-%d\n", bundle[pos].pc, state);
      //If the current state is normal fetching
      if(state == REGULAR) { 
         if(hammock_table->search_hammock(bundle[pos].pc, entry)) {
            terminated = true;   //Terminate the bundle at this hammock, if we find a hammock.
            bundle[pos].next_pc = INCREMENT_PC(bundle[pos].pc);
            state = PREDICATED_REGION; //Transition to then region. 
            bundle[pos].branch_type = HAMMOCK;
            bundle[pos].region_type = NORMAL;
            bundle[pos].branch = true;
            bundle[pos].is_hammock = true;
            //printf("Found HAMMOCK PC %llx\n", bundle[pos].pc);
            create_cmovs(entry);
            pos++;
         
         } //End - Hammock Found
         
         else { //Continue normal lookup in BTB.
            // convert {pc, pos} to {btb_bank, btb_pc}
            convert(pc, pos, btb_bank, btb_pc);

            // Search for the instruction in its bank.
            if (search(btb_bank, btb_pc, set, way)) {
               // BTB hit.  The BTB coordinates of this branch are {btb_bank, set, way}.
               bundle[pos].branch = true;
               bundle[pos].branch_type = btb[btb_bank][set][way].branch_type;
               bundle[pos].branch_target = btb[btb_bank][set][way].target;
               bundle[pos].region_type = NORMAL;
               bundle[pos].is_hammock = false;

               // Update LRU.
	            update_lru(btb_bank, set, way);

               // (1) Determine the instruction's next_pc field (i.e., pc of the next instruction, which may be in the same bundle or at the start of the next bundle).
               // (2) Determine if this is the last instruction in the bundle.
               //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
               // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
	            switch (bundle[pos].branch_type) {
	               case BTB_BRANCH:
	               // Increment the number of conditional branches in the fetch bundle.
	                  num_cond_branch++;

	               // The low two bits of cb_predictions correspond to the next two-bit counter to examine (because we shift it right, subsequently).
	               // From this two-bit counter, set the taken flag, accordingly.
	                  taken = ((cb_predictions & 3) >= 2);

	               // Shift out the used-up 2-bit counter, to set up prediction of the next conditional branch.
	                  cb_predictions = (cb_predictions >> 2);

	               // (1) Determine the instruction's next_pc field.
	                  bundle[pos].next_pc = (taken ?  btb[btb_bank][set][way].target : INCREMENT_PC(bundle[pos].pc));

	               // (2) Determine if this is the last instruction in the bundle.
	               //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
	                  if (taken || (num_cond_branch == cond_branch_per_cycle))
	                     terminated = true;

	                  break;

                  case BTB_JUMP_DIRECT:
                     // (1) Determine the instruction's next_pc field.
                     bundle[pos].next_pc = btb[btb_bank][set][way].target;

                     // (2) Determine if this is the last instruction in the bundle.
                     //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
                     terminated = true;
                     break;

                  case BTB_CALL_DIRECT:
                     // (1) Determine the instruction's next_pc field.
                     bundle[pos].next_pc = btb[btb_bank][set][way].target;

                     // (2) Determine if this is the last instruction in the bundle.
                     //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
                     terminated = true;

                     // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
                     update->push_ras = true;
                     update->push_ras_pc = INCREMENT_PC(bundle[pos].pc);
                     break;

                  case BTB_JUMP_INDIRECT:
                     // (1) Determine the instruction's next_pc field.
                     bundle[pos].next_pc = ib_predicted_target;

                     // (2) Determine if this is the last instruction in the bundle.
                     //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
                     terminated = true;
                     break;

                  case BTB_CALL_INDIRECT:
                     // (1) Determine the instruction's next_pc field.
                     bundle[pos].next_pc = ib_predicted_target;

                     // (2) Determine if this is the last instruction in the bundle.
                     //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
                     terminated = true;

                  // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
                     update->push_ras = true;
                     update->push_ras_pc = INCREMENT_PC(bundle[pos].pc);
                     break;

                  case BTB_RETURN:
                     // (1) Determine the instruction's next_pc field.
                     bundle[pos].next_pc = ras_predicted_target;

                     // (2) Determine if this is the last instruction in the bundle.
                     //     End the fetch bundle at any taken branch or at the maximum number of conditional branches.
                     terminated = true;

                     // (3) Record push_ras/pop_ras information in the "update" variable, which is needed by the Fetch Unit to speculatively update its RAS.
                     update->pop_ras = true;
                     break;

                  default:
                     // All possible branch types are enumerated above.
                     assert(0);
                     break;
	            }
            } //End - Hit in BTB
            else {
	            // BTB miss.
               bundle[pos].branch = false;
               bundle[pos].region_type = NORMAL;
               bundle[pos].is_hammock = false;
	            // (1) Determine the instruction's next_pc field.
	            bundle[pos].next_pc = INCREMENT_PC(bundle[pos].pc);
            } //End- Miss in BTB

            // Whether or not terminated was already set to true above, set it to true here, if the instruction cache posted an exception in this slot.
            if (bundle[pos].exception)
               terminated = true;

            // Go to the next instruction in the fetch bundle.
            pos++;

         } //End- When not hammock.
      } //End- Normal State


      //Current State is in Then Region.
      else if (state == PREDICATED_REGION) {
         bundle[pos].next_pc = INCREMENT_PC(bundle[pos].pc);
         bundle[pos].branch = false;
         bundle[pos].is_hammock = false;
         
         if(then_count < entry.then_length) { //Current Instruction is in then clause
            bundle[pos].region_type = THEN;
            then_count ++;
           // printf("In THEN, PC-%llx\n", bundle[pos].pc);
            //printf("Then Length %d\n", then_count);
         }
         else { //Instruction is in then region.
            if(entry.else_valid) {
               bundle[pos].region_type = ELSE;
               //printf("In ELSE, PC-%llx\n", bundle[pos].pc);
            }
         }

         if(bundle[pos].next_pc == entry.RPC) { //
            terminated = true;
            then_count = 0;
            state = CMOV_Region;
            //printf("FOUND RECONVERGENT PC %llx\n", bundle[pos].next_pc);
            //bundle[pos].next_pc = entry.RPC; //Change me.
         }
         pos++;
      } //End - PREIDCATED REGION

      else if(state == CMOV_Region) {
         if(cmov_instructions.size() > 1) { // Insert CMOVs till fetch width
            bundle[pos].cmov_log_reg = cmov_instructions.back();
            cmov_instructions.pop_back();
            bundle[pos].insn = insn_t(INSN_NOP);
            bundle[pos].region_type = CMOV;
            bundle[pos].branch = false;
            bundle[pos].is_hammock = false;
            //printf("In CMOV\n");
            pos++;
         }
         else if(cmov_instructions.size() == 1) { //If this was the last CMOV, then terminate the bundle and next PC=RPC
            bundle[pos].cmov_log_reg = cmov_instructions.back();
            cmov_instructions.pop_back();
            bundle[pos].insn = insn_t(INSN_NOP); //CHANGE ME. ASK
            bundle[pos].region_type = CMOV;
            bundle[pos].branch = false;
            bundle[pos].is_hammock = false;
            bundle[pos].next_pc = entry.RPC;
            terminated = true;
            state = REGULAR;
            //printf("In CMOV\n");
            //printf("INSERTING CMOV\n");
            pos++;
         }
      }


   }

   // Finalize the "update" variable (which is needed by the Fetch Unit to speculatively update its predictors and pc).
   // Above, we recorded values for the push_ras/pop_ras related fields.
   // Now, record values for num_cb (number of conditional branches in the fetch bundle) and next_pc (predicted PC of the next fetch bundle).
   assert(pos > 0);	// There must be at least one instruction in the fetch bundle.
   update->next_pc = bundle[pos-1].next_pc;
   update->num_cb = num_cond_branch;
   update->state = state;
   //printf("Bundle size- %d\n", pos-1);
   // Mark any residual slots in the fetch bundle as invalid (no instructions in those slots).
   while (pos < banks) {
      bundle[pos].valid = false;
      pos++;
   }
}


//
// pc: The start pc of the fetch bundle.
// pos: The position of the branch in the fetch bundle, that had missed in the BTB just prior.
//
// Role of this function: Add the branch to the BTB at the correct coordinates {btb_bank, set, way}.
//
void btb_t::update(uint64_t pc, uint64_t pos, insn_t insn) {
   uint64_t btb_bank;
   uint64_t btb_pc;
   uint64_t set;
   uint64_t way;
   uint64_t new_target;
   btb_branch_type_e new_branch_type;

   // decode the branch for it's type and taken target information
   new_branch_type = btb_t::decode(insn, (pc + (pos << 2)), new_target);
   //printf("UPDATING BTB PC %llx\n", pc);
   convert(pc, pos, btb_bank, btb_pc);	// convert {pc, pos} to {btb_bank, btb_pc}

   // Search for the instruction in its bank.
   // The BTB entry to replace (miss) or update (hit) is at coordinates {btb_bank, set, way}.
   // If it hits, assert that the reason for updating the entry is that the branch type changed or non-indirect branch's target changed
   // (self-modifying code or BTB was trained with data on the wrong-path beyond the text segment).
   bool btb_hit = search(btb_bank, btb_pc, set, way);
   if (btb_hit)
     assert((btb[btb_bank][set][way].branch_type != new_branch_type) ||
            ((insn.opcode() != OP_JALR) && (btb[btb_bank][set][way].target != new_target)));

   // The entry's metadata:
   btb[btb_bank][set][way].valid = true;
   btb[btb_bank][set][way].tag = (btb_pc >> log2sets);
   update_lru(btb_bank, set, way); // Update LRU.

   // The entry's payload:
   btb[btb_bank][set][way].branch_type = new_branch_type;
   btb[btb_bank][set][way].target = new_target;
}

void btb_t::invalidate(uint64_t pc, uint64_t pos) {
   uint64_t btb_bank;
   uint64_t btb_pc;
   uint64_t set;
   uint64_t way;
   //HP--printf("PC in BTB %llx\n", pc);
   convert(pc, pos, btb_bank, btb_pc);   // convert {pc, pos} to {btb_bank, btb_pc}
   
   // Search for the instruction in its bank.
   // The BTB entry to invalidate is at coordinates {btb_bank, set, way}.
   // The pipeline should not invalidate an entry that doesn't exist.
   bool btb_hit = search(btb_bank, btb_pc, set, way);
   assert(btb_hit);
   
   // Invalidate the entry and make it the LRU way of the set.
   btb[btb_bank][set][way].valid = false;
   
   for (uint64_t i = 0; i < assoc; i++) {
      if (btb[btb_bank][set][i].lru > btb[btb_bank][set][way].lru)
         btb[btb_bank][set][i].lru--;
   }
   btb[btb_bank][set][way].lru = assoc - 1;
}

////////////////////////////////////
// Private utility functions.
////////////////////////////////////

// Convert {pc, pos} to {btb_bank, btb_pc}, where:
// pc: start PC of a fetch bundle
// pos: position of the instruction within the fetch bundle
// btb_bank: which bank to search for this instruction.
// btb_pc: the pc of the instruction, shifted right to eliminate the low 2 bits (00) and the bank select bits after it.
void btb_t::convert(uint64_t pc, uint64_t pos, uint64_t &btb_bank, uint64_t &btb_pc) {
   // Calculate the btb bank that should be referenced.
   //
   // pc: The start pc of the fetch bundle. Shift "pc" right by 2 bits to get the instr.-level PC from the byte-level PC.   "(pc >> 2)"
   // pos: This is the instr. slot of interest in the fetch bundle.
   //      If 0, instr. is at (pc>>2)+0; if 1, instr. is at (pc>>2)+1; if 2, instr. is at (pc>>2)+2; etc.          "(pc >> 2) + pos"
   // banks: We asserted that this must be a power-of-2.
   //        Thus, BTB bank selection can be done by taking the instr. pc, "(pc >> 2) + pos", and masking it with "(banks - 1)".
   btb_bank = (((pc >> 2) + pos) & (banks - 1));

   // Discard the low two bits and the bank selection bits that follow it, from the instruction's PC.
   // The bank selection bits are implied by which bank is referenced.
   btb_pc = (((pc >> 2) + pos) >> log2banks);
}

// This function searches for the specified branch, "btb_pc", in the specified bank, "btb_bank".
// It returns true if found (hit) and false if not found (miss).
// It outputs the "set" and "way" of either (a) the branch's entry (hit) or (b) the LRU entry (which can be used by the caller for replacement).
bool btb_t::search(uint64_t btb_bank, uint64_t btb_pc, uint64_t &set, uint64_t &way) {
   // Break up btb_pc into index and tag.
   uint64_t index = (btb_pc & (sets - 1));
   uint64_t tag = (btb_pc >> log2sets);

   // Search the indexed set.
   bool hit = false;
   uint64_t hit_way = assoc; // out-of-bounds
   uint64_t lru_way = assoc; // out-of-bounds
   for (uint64_t i = 0; i < assoc; i++) {
      if (btb[btb_bank][index][i].valid && (btb[btb_bank][index][i].tag == tag)) {
         hit = true;
	 hit_way = i;
	 break;
      }
      else if (btb[btb_bank][index][i].lru == (assoc - 1)) {
         lru_way = i;
      }
   }

   // Outputs.
   set = index;
   way = (hit ? hit_way : lru_way);
   assert(way < assoc);
   return(hit);
}


void btb_t::update_lru(uint64_t btb_bank, uint64_t set, uint64_t way) {
   // Make "way" most-recently-used.
   for (uint64_t i = 0; i < assoc; i++) {
      if (btb[btb_bank][set][i].lru < btb[btb_bank][set][way].lru)
         btb[btb_bank][set][i].lru++;
   }
   btb[btb_bank][set][way].lru = 0;
}


btb_branch_type_e btb_t::decode(insn_t insn, uint64_t pc, uint64_t &target) {
   btb_branch_type_e branch_type;
   switch (insn.opcode()) {
      case OP_BRANCH:
         branch_type = BTB_BRANCH;
	 target = BRANCH_TARGET;
         break;

      case OP_JAL:
         // According to the ABI, a JAL that saves its return address into X1 is a call instruction.
         branch_type = ((insn.rd() == 1) ?  BTB_CALL_DIRECT : BTB_JUMP_DIRECT);
	 target = JUMP_TARGET;
         break;

      case OP_JALR:
         if ((insn.rd() == 0) && (insn.rs1() == 1)) {
	    // According to the ABI, a JALR that discards its return address (writes X0) and jumps to the link register (X1) is a return instruction.
	    branch_type = BTB_RETURN;
         }
	 else if (insn.rd() == 1) {
	    // According to the ABI, a JALR that saves its return address into X1 is a call instruction.
	    branch_type = BTB_CALL_INDIRECT;
         }
	 else {
            branch_type = BTB_JUMP_INDIRECT;
	 }
	 // Target is dynamic and therefore unknown
	 target = 0;
         break;

      default:
         assert(0);
         break;
   }
   return(branch_type);
}




#if 0
// RISCV ISA spec, Table 2.1, explains rules for inferring a call instruction.
if (is_link_reg(insn.rd()))
   ras.push(pc + 4);

bool bpu_t::is_link_reg(uint64_t x) {
   return((x == 1) || (x == 5));
}
#endif
