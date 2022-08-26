#include "pipeline.h"
#include "debug.h"
extern bool logging_on;


void pipeline_t::check_single(reg_t micro, reg_t isa, db_t* actual, const char *desc) {
   if (micro != isa) {
      #ifdef RISCV_MICRO_DEBUG
        logging_on = true;
        PAY.dump(this, PAY.head, stderr);
        pipe->dump(this, actual, stderr);
      #endif
      printf("Instruction %.0f, Cycle %.0f: %s Pipeline:%" PRIreg " vs. isaSim:%" PRIreg ".\n", (double)num_insn, (double)cycle, desc, micro, isa);
      assert(0);
   }
}

void pipeline_t::check_double(reg_t micro0, reg_t micro1, reg_t isa0, reg_t isa1, const char *desc) {
   if ((micro0 != isa0) || (micro1 != isa1)) {
      logging_on = true;
      printf("Instruction %.0f, Cycle %.0f: %s Pipeline:%" PRIreg ",%" PRIreg " vs. isaSim:%" PRIreg ",%" PRIreg ".\n", (double)num_insn, (double)cycle, desc, micro0, micro1, isa0, isa1);
      assert(0);
   }
}

void pipeline_t::check_state(state_t* micro_state, state_t* isa_state, db_t* actual) {
   bool fail = false;

   //if(micro_state->epc               !=  isa_state->epc              ) fail = true;
   if(micro_state->badvaddr          !=  isa_state->badvaddr         ) fail = true;
   //if(micro_state->evec              !=  isa_state->evec             ) fail = true;
   //if(micro_state->ptbr              !=  isa_state->ptbr             ) fail = true;
   //if(micro_state->pcr_k0            !=  isa_state->pcr_k0           ) fail = true;
   //if(micro_state->pcr_k1            !=  isa_state->pcr_k1           ) fail = true;
   //if(micro_state->cause             !=  isa_state->cause            ) fail = true;
   if(micro_state->tohost            !=  isa_state->tohost           ) fail = true;
   if(micro_state->fromhost          !=  isa_state->fromhost         ) fail = true;
   if(micro_state->count             !=  isa_state->count            ) fail = true;
   //if(micro_state->compare           !=  isa_state->compare          ) fail = true;
   if(micro_state->sr                !=  isa_state->sr               ) fail = true;
   if(micro_state->fflags            !=  isa_state->fflags           ) fail = true;
   if(micro_state->frm               !=  isa_state->frm              ) fail = true;
   //if(micro_state->load_reservation  !=  isa_state->load_reservation ) fail = true;

   if (fail) {
      #ifdef RISCV_MICRO_DEBUG
        logging_on = true;
        fprintf(stderr,"State for micro_sim:\n");
        PAY.dump(this, PAY.head, stderr);
        micro_state->dump(stderr);
        fprintf(stderr,"\nState for isa_sim:\n");
        pipe->dump(this, actual, stderr);
      #endif

	     if(micro_state->badvaddr          !=  isa_state->badvaddr         ) fprintf(stderr, "badvaddr %x\n", micro_state->badvaddr);
   //if(micro_state->evec              !=  isa_state->evec             ) fail = true;
   //if(micro_state->ptbr              !=  isa_state->ptbr             ) fail = true;
   //if(micro_state->pcr_k0            !=  isa_state->pcr_k0           ) fail = true;
   //if(micro_state->pcr_k1            !=  isa_state->pcr_k1           ) fail = true;
   //if(micro_state->cause             !=  isa_state->cause            ) fail = true;
   if(micro_state->tohost            !=  isa_state->tohost           ) fprintf(stderr, "tohost %x\n", micro_state->tohost);
   if(micro_state->fromhost          !=  isa_state->fromhost         ) fprintf(stderr, "fromhost %x\n", micro_state->fromhost);
   if(micro_state->count             !=  isa_state->count            ) fprintf(stderr, "count %x\n", micro_state->count);
   //if(micro_state->compare           !=  isa_state->compare          ) fail = true;
   if(micro_state->sr                !=  isa_state->sr               ) fprintf(stderr, "sr %x\n", micro_state->sr);
   if(micro_state->fflags            !=  isa_state->fflags           ) fprintf(stderr, "fflags %x\n", micro_state->fflags);
   if(micro_state->frm               !=  isa_state->frm              ) fprintf(stderr, "frm %x\n", micro_state->frm);
      isa_state->dump(stderr);
      printf("Instruction %.0f, Cycle %.0f: State check failed.\n", (double)num_insn, (double)cycle);
      assert(0);
   }
}

void pipeline_t::checker() {

   #ifdef RISCV_MICRO_DEBUG
    fflush(0);
   #endif

	 unsigned int head;	// Index of the head instruction in PAY.
	 db_t *actual;		// Pointer to corresponding instruction in the functional simulator.

	 // Get the index of the head instruction in PAY.
	 head = PAY.head;

   // Get pointer to the corresponding instruction in the functional simulator.
	 // This enables checking results of the pipeline simulator.
	 assert(PAY.buf[head].good_instruction && (PAY.buf[head].db_index != DEBUG_INDEX_INVALID));
	 if (PAY.buf[head].split && PAY.buf[head].upper)
	    actual = pipe->peek(PAY.buf[head].db_index);
	 else
	    actual = pipe->pop(PAY.buf[head].db_index);

	 // Validate the instruction PC.
	 check_single(PAY.buf[head].pc, actual->a_pc, actual, "PC mismatch.");

   check_state(this->get_state(),actual->a_state,actual);

   // If an architectural exception
   // Make sure that MICRO_SIM also excepts but
   // don't check anything else
   if(actual->a_exception){
     // TODO: Add this check
     assert(REN->get_exception(PAY.buf[head].AL_index) == true);
   }
   // If not an architectural exception
   else{
	   if (PAY.buf[head].split) {
	      switch (PAY.buf[head].inst.opcode()) {

	         default:
	            assert(0);
	            break;
	      }
	   }
	   else if (IS_MEM_OP(PAY.buf[head].flags)) {
	      // Validate address.
	      check_single((reg_t)PAY.buf[head].addr, (reg_t)actual->a_addr, actual, "Load/store address mismatch.");

	      // Validate first source register.
	      check_single(PAY.buf[head].A_value.dw, actual->a_rsrc[0].value, actual, "Load/store first source mismatch.");

	      if ((IS_STORE(PAY.buf[head].flags))){
	         // Validate second source register.
			 //HP--printf("Check Single - Logical Source Registers - %d, %d %d\n", PAY.buf[head].A_log_reg, PAY.buf[head].B_log_reg, PAY.buf[head].D_log_reg);
			 //HP--printf("DEbug Values - %d %d %d", actual->a_rsrc[0].n, actual->a_rsrc[1].n, actual->a_rsrc[2].n);
	         check_single(PAY.buf[head].B_value.dw, actual->a_rsrc[1].value, actual, "Store second source mismatch.");
	      }

	      if (IS_LOAD(PAY.buf[head].flags)) {
	         // Validate destination register.
	         check_single(PAY.buf[head].C_value.dw, actual->a_rdst[0].value, actual, "Load destination mismatch.");
	      }
	   }
	   else {
	      assert(actual->a_num_rsrcA == 0);	// already checked all loads/stores above (whether split or regular)

	      switch (PAY.buf[head].inst.opcode()) {
	         //case DMTC1:
	         //   check_single(PAY.buf[head].A_value.w[0], actual->a_rsrc[0].value, "First source mismatch.");
	         //   check_single(PAY.buf[head].B_value.w[0], actual->a_rsrc[1].value, "Second source mismatch.");
	         //   check_double(PAY.buf[head].C_value.w[0], PAY.buf[head].C_value.w[1], actual->a_rdst[0].value, actual->a_rdst[1].value, "Destination mismatch.");
	         //   break;

	         //case FADD_D: case FSUB_D: case FMUL_D: case FDIV_D:
	         //   check_double(PAY.buf[head].A_value.w[0], PAY.buf[head].A_value.w[1], actual->a_rsrc[0].value, actual->a_rsrc[1].value, "First source mismatch.");
	         //   check_double(PAY.buf[head].B_value.w[0], PAY.buf[head].B_value.w[1], actual->a_rsrc[2].value, actual->a_rsrc[3].value, "Second source mismatch.");
	         //   check_double(PAY.buf[head].C_value.w[0], PAY.buf[head].C_value.w[1], actual->a_rdst[0].value, actual->a_rdst[1].value, "Destination mismatch.");
	         //   break;

	         //case FABS_D: case FNEG_D: case FMOV_D: case FSQRT_D:
	         //   check_double(PAY.buf[head].A_value.w[0], PAY.buf[head].A_value.w[1], actual->a_rsrc[0].value, actual->a_rsrc[1].value, "Source mismatch.");
	         //   check_double(PAY.buf[head].C_value.w[0], PAY.buf[head].C_value.w[1], actual->a_rdst[0].value, actual->a_rdst[1].value, "Destination mismatch.");
	         //   break;

	         //case CVT_S_D: case CVT_W_D:
	         //   check_double(PAY.buf[head].A_value.w[0], PAY.buf[head].A_value.w[1], actual->a_rsrc[0].value, actual->a_rsrc[1].value, "Source mismatch.");
	         //   check_single(PAY.buf[head].C_value.w[0], actual->a_rdst[0].value, "Destination mismatch.");
	         //   break;

	         //case CVT_D_S: case CVT_D_W:
	         //   check_single(PAY.buf[head].A_value.w[0], actual->a_rsrc[0].value, "Source mismatch.");
	         //   check_double(PAY.buf[head].C_value.w[0], PAY.buf[head].C_value.w[1], actual->a_rdst[0].value, actual->a_rdst[1].value, "Destination mismatch.");
	         //   break;

	         //case C_EQ_D: case C_LT_D: case C_LE_D:
	         //   check_double(PAY.buf[head].A_value.w[0], PAY.buf[head].A_value.w[1], actual->a_rsrc[0].value, actual->a_rsrc[1].value, "First source mismatch.");
	         //   check_double(PAY.buf[head].B_value.w[0], PAY.buf[head].B_value.w[1], actual->a_rsrc[2].value, actual->a_rsrc[3].value, "Second source mismatch.");
	         //   check_single(PAY.buf[head].C_value.w[0], actual->a_rdst[0].value, "Destination mismatch.");
	         //   break;

	         default:
	            // Remaining instructions handled here:
	            // All source and destination operands are either integer or single-precision float.
	            assert(actual->a_num_rsrc <= D_MAX_RSRC);
	            assert(actual->a_num_rdst <= D_MAX_RDST);

	            if (actual->a_num_rsrc > 0) {
	               // Validate first source register.
	               check_single(PAY.buf[head].A_value.dw, actual->a_rsrc[0].value, actual, "First source mismatch.");
	            }
	            if (actual->a_num_rsrc > 1) {
	               // Validate second source register.
	               check_single(PAY.buf[head].B_value.dw, actual->a_rsrc[1].value, actual, "Second source mismatch.");
	            }
	            if (actual->a_num_rdst > 0) {
	               // Validate destination register.
	               check_single(PAY.buf[head].C_value.dw, actual->a_rdst[0].value, actual, "Destination mismatch.");
	            }
	            break;
	      }
	   }
   }
}


