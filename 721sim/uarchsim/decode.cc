#include "pipeline.h"


void pipeline_t::decode() {
	unsigned int i;
	unsigned int index;
	insn_t inst;

	// Stall the Decode Stage if there is not enough space in the Fetch Queue for 2x the fetch bundle width.
	// The factor of 2x assumes that each instruction in the fetch bundle is split, in the worst case.

	// Count the number of instructions in the fetch bundle.
	for (i = 0; i < fetch_width; i++){
		if (!DECODE[i].valid) {
			break;
		}
  }

	// Stall if there is not enough space in the Fetch Queue.
	if (!FQ.enough_space(i<<1)) {
		return;
	}


	for (i = 0; i < fetch_width; i++) {

		if (DECODE[i].valid) {
			DECODE[i].valid = false;    // Valid instruction: Decode it and remove it from the pipeline register.
		}
		else {
			break;    // Not a valid instruction: Reached the end of the fetch bundle so exit loop.
		}

		index = DECODE[i].index;

		// Get instruction from payload buffer.
    inst = PAY.buf[index].inst;

    LOG(decode_log,cycle,PAY.buf[index].sequence,PAY.buf[index].pc,"Instruction: %08" PRIX32 "",(word_t)inst.bits());


		// Set checkpoint flag.
		switch (inst.opcode()) {
			case OP_JAL:
			case OP_JALR:
			case OP_BRANCH:
				//Don't Need to Checkpoint DHP branches. --------------------------------------------------
				if(PAY.buf[index].is_hammock || PAY.buf[index].instruction_type == THEN) PAY.buf[index].checkpoint = false;
				else PAY.buf[index].checkpoint = true;
				break;

			default:
				PAY.buf[index].checkpoint = false;
				break;
		}

		// Set flags  and function units
		switch(inst.opcode()) {

			case OP_JAL:
			case OP_JALR:
				PAY.buf[index].flags = (F_CTRL|F_UNCOND);
				PAY.buf[index].fu = FU_BR;
				break;

			case OP_BRANCH:
				PAY.buf[index].flags = (F_CTRL|F_COND);
				PAY.buf[index].fu = FU_BR;
				break;

			case OP_LOAD:
				PAY.buf[index].flags = (F_MEM|F_LOAD|F_DISP);
				PAY.buf[index].fu = FU_LS;
				break;

			case OP_STORE:
				PAY.buf[index].flags = (F_MEM|F_STORE|F_DISP);
				PAY.buf[index].fu = FU_LS;
				break;

			case OP_OP:
			case OP_OP_32:  // valid only in 64bit mode - illegal inst exception in 32 bit mode
				PAY.buf[index].flags = (F_ICOMP);
				PAY.buf[index].fu = FU_ALU_S;

				if(inst.funct7() == FN7_MULDIV) {
					PAY.buf[index].flags = (F_ICOMP|F_LONGLAT);
					PAY.buf[index].fu = FU_ALU_C;
				}
				break;

			case OP_OP_IMM:
			case OP_OP_IMM_32: // valid only in 64bit mode - illegal inst exception in 32 bit mode
			case OP_LUI:
			case OP_AUIPC:
				PAY.buf[index].flags = (F_ICOMP);
				PAY.buf[index].fu = FU_ALU_S;
				break;

      // Set both F_MEM and F_FMEM so that they go to the MEM lane
      // but the FP unit requirement also gets checked in DISPATCH.
			case OP_LOAD_FP:
				PAY.buf[index].flags = (F_MEM|F_FMEM|F_LOAD|F_DISP);
				PAY.buf[index].fu = FU_LS_FP;
				break;

      // Set both F_MEM and F_FMEM so that they go to the MEM lane
      // but the FP unit requirement also gets checked in DISPATCH.
			case OP_STORE_FP:
				PAY.buf[index].flags = (F_MEM|F_FMEM|F_STORE|F_DISP);
				PAY.buf[index].fu = FU_LS_FP;
				break;

			case OP_OP_FP:
      case OP_MADD:
      case OP_MSUB:
      case OP_NMADD:
      case OP_NMSUB:
				PAY.buf[index].flags = (F_FCOMP);
				PAY.buf[index].fu = FU_ALU_FP;
				break;

//      	 case FMUL_S: case FMUL_D: case FDIV_S: case FDIV_D: case FSQRT_S: case FSQRT_D:
//      	    PAY.buf[index].flags = (F_FCOMP|F_LONGLAT);
//      	    break;

			case OP_SYSTEM:
        // Currently all SYSTEM ops flush the pipeline like an exception.
        // CSRxxx instructions do not invoke a exception handler whereas
        // the others do.
				PAY.buf[index].flags = (F_TRAP|F_CSR);
				PAY.buf[index].fu = FU_ALU_S;
				break;

			case OP_MISC_MEM:
        // Currently all SYSTEM ops flush the pipeline like an exception.
        // CSRxxx instructions do not invoke a exception handler whereas
        // the others do.
				PAY.buf[index].flags = (F_TRAP);
				PAY.buf[index].fu = FU_ALU_S;
				break;

			case OP_AMO:
        switch(inst.funct5()){
          case FN5_AMO_LR:
    				PAY.buf[index].flags = (F_MEM|F_LOAD|F_DISP|F_AMO);
		    		PAY.buf[index].fu = FU_LS;
            break;
          case FN5_AMO_SC:
    				PAY.buf[index].flags = (F_MEM|F_STORE|F_DISP|F_AMO);
		    		PAY.buf[index].fu = FU_LS;
            break;
          default:
    				PAY.buf[index].flags = (F_AMO);
		    		PAY.buf[index].fu = FU_ALU_S;
            //assert(0);
        }
				break;

			default:
				//assert(0);
				break;
		}


		// Set register operands and split instructions.
		// Select IQ.

		// Default values.
		PAY.buf[index].split = false;
		PAY.buf[index].split_store = false;
		PAY.buf[index].A_valid = false;
		PAY.buf[index].B_valid = false;
		PAY.buf[index].C_valid = false;
		PAY.buf[index].D_valid = false;
		PAY.buf[index].iq = SEL_IQ;

		switch (inst.opcode()) {

			case OP_JAL:
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd();  // This should be either x1 or x0 as per software calling conventions
				break;

			case OP_JALR:
				// source register
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd();
				break;

			case OP_BRANCH:
				// first source register
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();
				// second source register
				PAY.buf[index].B_valid = true;
				PAY.buf[index].B_log_reg = inst.rs2();
				// Destination (Predicate Register) for Hammock.
				if(PAY.buf[index].branch_type == HAMMOCK) {
					PAY.buf[index].C_valid = true;
					PAY.buf[index].C_log_reg = 64;
				}
				break;


			case OP_LOAD:
				// base register for AGEN
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd();
				break;


			case OP_LOAD_FP:
				// base register for AGEN
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd()+NXPR;
				break;

			case OP_STORE:
				// base register for AGEN
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();
				// source register
				PAY.buf[index].B_valid = true;
				PAY.buf[index].B_log_reg = inst.rs2();
				break;


			case OP_STORE_FP:
				// base register for AGEN
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();

				// source register
				PAY.buf[index].B_valid = true;
				PAY.buf[index].B_log_reg = inst.rs2()+NXPR;
				break;

			case OP_OP:
			case OP_OP_32:  // valid only in 64bit mode - illegal inst exception in 32 bit mode
				// first source register
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1();
				// second source register
				PAY.buf[index].B_valid = true;
				PAY.buf[index].B_log_reg = inst.rs2();
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd();
				break;


			case OP_OP_IMM:
			case OP_OP_IMM_32:  // valid only in 64bit mode - illegal inst exception in 32 bit mode
        if(inst.bits() == INSN_NOP){
          // Select IQ.
		  if(PAY.buf[index].instruction_type == CMOV) {
			  PAY.buf[index].A_valid = true;
			  PAY.buf[index].A_log_reg = PAY.buf[index].CMOV_log_reg; //RS1
			  PAY.buf[index].B_valid = true;
			  PAY.buf[index].B_log_reg = PAY.buf[index].CMOV_log_reg; //RS2
			  PAY.buf[index].C_valid = true;
			  PAY.buf[index].C_log_reg = PAY.buf[index].CMOV_log_reg; //RD
			  PAY.buf[index].D_valid = true;
			  PAY.buf[index].D_log_reg = 64; //Predicate Register for CMOV Type
			  PAY.buf[index].iq = SEL_IQ;
		  }
          else {
			  PAY.buf[index].iq = SEL_IQ_NONE;

	  // 3/20/19: Fix for checker.
          PAY.buf[index].A_valid = true;
          PAY.buf[index].A_log_reg = inst.rs1();
	  assert(PAY.buf[index].A_log_reg == 0);
          PAY.buf[index].A_value.dw = 0;
		  }
		}
		 else {
			
				  // source register
				  PAY.buf[index].A_valid = true;
				  PAY.buf[index].A_log_reg = inst.rs1();
				  // dest register
				  PAY.buf[index].C_valid = true;
				  PAY.buf[index].C_log_reg = inst.rd();
        }
				break;


      case OP_OP_FP:
        switch(inst.funct5()){
          case FN5_FADD:  case FN5_FSUB:      case FN5_FMUL:  case FN5_FDIV:
          case FN5_FSGNJ: case FN5_FMIN_MAX: 
				    // first source register
				    PAY.buf[index].A_valid = true;
				    PAY.buf[index].A_log_reg = inst.rs1()+NXPR;
				    // second source register
				    PAY.buf[index].B_valid = true;
				    PAY.buf[index].B_log_reg = inst.rs2()+NXPR;
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd()+NXPR;
				    break;

          case FN5_FCOMP:
				    // first source register
				    PAY.buf[index].A_valid = true;
				    PAY.buf[index].A_log_reg = inst.rs1()+NXPR;
				    // second source register
				    PAY.buf[index].B_valid = true;
				    PAY.buf[index].B_log_reg = inst.rs2()+NXPR;
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd();
				    break;

          case FN5_FSQRT:  case FN5_FCVT_DS:
				    // first source register
				    PAY.buf[index].A_valid = true;
				    PAY.buf[index].A_log_reg = inst.rs1()+NXPR;
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd()+NXPR;
				    break;

          case FN5_FCVT_I2FP: case FN5_FMV_I2FP: 
				    // first source register
				    PAY.buf[index].A_valid = true;
				    PAY.buf[index].A_log_reg = inst.rs1();
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd()+NXPR;
				    break;

          case FN5_FCVT_FP2I: case FN5_FMV_FP2I: 
				    // first source register
				    PAY.buf[index].A_valid = true;
				    PAY.buf[index].A_log_reg = inst.rs1()+NXPR;
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd();
				    break;
		
          default:
            break;
        }
        break;
       //TODO
			/**** Decode FMOV instructions as they are special
			 * The renamer can have a unified RMT with lower 32 integer RMT entries
			 * and upper 32 FP RMT entries.
			 ******/


      // System instructions flow through the pipeline without making any changes
      // until they are committed. The system registers are written or read 
			case OP_SYSTEM:
        switch(inst.funct3()){
          case FN3_CLR:
          case FN3_RW:
          case FN3_SET:
				    // first source register
            // Used as immediate in case of IMM form of the instructions
				    PAY.buf[index].A_valid = true;
				    PAY.buf[index].A_log_reg = inst.rs1();
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd();
            // CSR address
				    PAY.buf[index].CSR_addr = inst.csr();
            break;
          case FN3_CLR_IMM:
          case FN3_RW_IMM:
          case FN3_SET_IMM:
				    // first source register field is used as immediate 
            // in case of IMM form of the instructions
				    PAY.buf[index].A_valid = false;
				    PAY.buf[index].A_log_reg = inst.rs1();
				    // dest register
				    PAY.buf[index].C_valid = true;
				    PAY.buf[index].C_log_reg = inst.rd();
            // CSR address
				    PAY.buf[index].CSR_addr = inst.csr();
            break;
          case FN3_SC_SB:
            if(inst.funct12() == FN12_SRET){
				      PAY.buf[index].CSR_addr = CSR_STATUS;
            }
            else {
  				    // Select IQ.
	  			    PAY.buf[index].iq = SEL_IQ_NONE;
	  			    if (inst.funct12() == FN12_SCALL)
	  			       PAY.buf[index].trap.post(trap_syscall());
	  			    else if (inst.funct12() == FN12_SBREAK)
	  			       PAY.buf[index].trap.post(trap_breakpoint());
	  			    else
				       PAY.buf[index].trap.post(trap_illegal_instruction());
            }
            break;
          default:
            PAY.buf[index].iq = SEL_IQ_NONE;
            PAY.buf[index].trap.post(trap_illegal_instruction());
            break;
        }         
				break;

      // Ignores zeroed out fields (rs1,rd and imm[11:8] for forward compatibility.
      // Ignores successor and predecessor fields and does a global FENCE for all types of fences.
      // D-Cache should be flushed at FENCE
      // D-Cache should be flushed and I-Cache should be flushed at FENCE.I
      // TODO: Should go to SEL_IQ_NONE_EXCEPTION when fence is actually implemented.
      // Current implmentation is trivial.
      case OP_MISC_MEM:
				PAY.buf[index].iq = SEL_IQ_NONE;
        break;

			case OP_AMO:
				if (inst.funct3() == FN3_AMO_D || inst.funct3() == FN3_AMO_W) {
					switch (inst.funct5()) {
						case FN5_AMO_LR:
							// base register for AGEN
							PAY.buf[index].A_valid = true;
							PAY.buf[index].A_log_reg = inst.rs1();
							// dest register
							PAY.buf[index].C_valid = true;
							PAY.buf[index].C_log_reg = inst.rd();
							break;
						case FN5_AMO_SC:
							// base register for AGEN
							PAY.buf[index].A_valid = true;
							PAY.buf[index].A_log_reg = inst.rs1();
							// source register
							PAY.buf[index].B_valid = true;
							PAY.buf[index].B_log_reg = inst.rs2();
							// dest register
							PAY.buf[index].C_valid = true;
							PAY.buf[index].C_log_reg = inst.rd();
							break;
						case FN5_AMO_SWAP:
						case FN5_AMO_ADD:
						case FN5_AMO_XOR:
						case FN5_AMO_AND:
						case FN5_AMO_OR:
						case FN5_AMO_MIN:
						case FN5_AMO_MAX:
						case FN5_AMO_MINU:
						case FN5_AMO_MAXU:
							// base register for address
							PAY.buf[index].A_valid = true;
							PAY.buf[index].A_log_reg = inst.rs1();
							// source register
							PAY.buf[index].B_valid = true;
							PAY.buf[index].B_log_reg = inst.rs2();
							// dest register
							PAY.buf[index].C_valid = true;
							PAY.buf[index].C_log_reg = inst.rd();
							break;
						default:
							PAY.buf[index].iq = SEL_IQ_NONE;
							PAY.buf[index].trap.post(trap_illegal_instruction());
							break;
					}
				} else {
					PAY.buf[index].iq = SEL_IQ_NONE;
					PAY.buf[index].trap.post(trap_illegal_instruction());
				}
        break;

			case OP_LUI:
			case OP_AUIPC:
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd();
				break;

      case OP_MADD:
      case OP_MSUB:
      case OP_NMADD:
      case OP_NMSUB:
				// first source register
				PAY.buf[index].A_valid = true;
				PAY.buf[index].A_log_reg = inst.rs1()+NXPR;
				// second source register
				PAY.buf[index].B_valid = true;
				PAY.buf[index].B_log_reg = inst.rs2()+NXPR;
				// third source register
				PAY.buf[index].D_valid = true;
				PAY.buf[index].D_log_reg = inst.rs3()+NXPR;
				// dest register
				PAY.buf[index].C_valid = true;
				PAY.buf[index].C_log_reg = inst.rd()+NXPR;
				break;

			default:
        // Unknown opcode: do not dispatch to IQ.
        // This is just to make sure none of the asserts in the pipeline
        // fire on seeing an unknown instruction. This instruction should
        // never get committed as this part of code can only be reached
        // if fetch has been redirected by bad branch prediction to a
        // memory region containing random values or zeroes.
                                PAY.buf[index].iq = SEL_IQ_NONE;
				break;
		}

    //If destination X0 (Not F0), remove the destination as X0 should never be written to
    //or renamed for that matter. Set C_valid to 0 
    if(PAY.buf[index].C_log_reg == 0){
  	  PAY.buf[index].C_valid = false;
    }

		// Decode some details about loads and stores:
		// size and sign of data, and left/right info.
		switch (inst.opcode()) {
			case OP_LOAD:
			case OP_STORE:
			case OP_LOAD_FP:
			case OP_STORE_FP:
			case OP_AMO:
				PAY.buf[index].size = inst.ldst_size();      // Load size is encoded in funct3/width[1:0] field or inst[13:12]
				PAY.buf[index].is_signed = inst.ldst_sign(); // Load sign is encoded in funct3/width[2] field or inst[14]
				PAY.buf[index].left = false;
				PAY.buf[index].right = false;
				break;

			default:
				break;
		}

		// Insert one or two instructions into the Fetch Queue (indices).
		FQ.push(index);
		if (PAY.buf[index].split) {
      // Should not come here in current 721sim, with unified int/fp pipeline.
      // Will need this functionality for split-stores, however.
      assert(0);
			assert(PAY.buf[index+1].split);
			assert(PAY.buf[index].upper);
			assert(!PAY.buf[index+1].upper);
			FQ.push(index+1);
		}


    #ifdef RISCV_MICRO_DEBUG
      // Dump debug info if needed
      //Pass a pointer to the processor
    
      PAY.dump(this,index,decode_log);
    #endif


	}
}
