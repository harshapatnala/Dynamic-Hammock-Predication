#ifndef PAYLOAD_H
#define PAYLOAD_H

//#include "debug.h"
#include "trap.h"
#include "decode.h"
#include "fu.h"
#include "fetchunit_types.h"
#include <cstdio>

#define PAYLOAD_BUFFER_SIZE  8192


typedef
enum {
   SEL_IQ,			// Select unified IQ.
   SEL_IQ_NONE,	// Skip IQ, mark completed right away. Set this if detected any exceptions before the DISPATCH stage.
} sel_iq;

union union64_t {
   reg_t dw;
   sreg_t sdw;
   word_t w[2];
   sword_t sw[2];
   float f[2];
   double d;
};

typedef unsigned int debug_index_t;

class trap_storage_t {
private:
	union {
		char _0[sizeof(trap_t)];
		char _1[sizeof(mem_trap_t)];
		char _2[sizeof(trap_instruction_address_misaligned)];
		char _3[sizeof(trap_instruction_access_fault)];
		char _4[sizeof(trap_illegal_instruction)];
		char _5[sizeof(trap_privileged_instruction)];
		char _6[sizeof(trap_fp_disabled)];
		char _7[sizeof(trap_syscall)];
		char _8[sizeof(trap_breakpoint)];
		char _9[sizeof(trap_load_address_misaligned)];
		char _10[sizeof(trap_store_address_misaligned)];
		char _11[sizeof(trap_load_access_fault)];
		char _12[sizeof(trap_store_access_fault)];
		char _13[sizeof(trap_accelerator_disabled)];
		char _14[sizeof(trap_csr_instruction)];
	} trap_storage = {};
	bool content_valid = false;

public:
	trap_t *get();

	void post(const trap_t *t);

	void post(const trap_t &t);

	void clear();

	bool valid() { return content_valid; };
};

typedef struct {

   ////////////////////////
   // Set by Fetch1 Stage.
   ////////////////////////
   //------------- ADDED INSTRUCTION TYPE -----------
   inst_region_e instruction_type;
   bool is_hammock;
   uint64_t CMOV_log_reg;
   int predication_tag;
   //-----------------------------------------------

   insn_t inst;                 // The RISCV instruction.
   reg_t pc;                    // The instruction's PC.
   reg_t next_pc;               // The next instruction's PC. (I.e., the PC of the instruction fetched after this one.)
   bool branch;				// This instruction was identified as a branch, by the BTB (if bundle came from instr. cache) or by the trace cache.
   btb_branch_type_e branch_type;	// If the instruction was identified as a branch, this is its type.
   uint64_t branch_target;        // If the instruction was identified as a branch, this is its taken target (not valid for indirect branches).

   bool good_instruction;       // If 'true', this instruction has a
                                // corresponding instruction in the
                                // functional simulator. This implies the
                                // instruction is on the correct control-flow
                                // path.

   debug_index_t db_index;      // Index of corresponding instruction in the
                                // functional simulator
                                // (if good_instruction == 'true').
                                // Having this index is useful for obtaining
                                // oracle information about the instruction,
                                // for various oracle modes of the simulator.

   // FIX_ME: not currently set/incremented
   uint64_t sequence;           // Unique sequence number for speculatively
                                // fetched instructions.  Helpful for
                                // logging (debug traces).

   ////////////////////////
   // Set by Fetch2 Stage.
   ////////////////////////

   unsigned int pred_tag;       // If the instruction is a branch, this is its
                                // index into the Fetch Unit's branch queue.

   ////////////////////////
   // Set by Decode Stage.
   ////////////////////////

   unsigned int flags;          // Operation flags: can be used for quickly
                                // deciphering the type of instruction.
   fu_type fu;                  // Operation function unit type.
   cycle_t latency;             // Operation latency (ignore: not currently used).

   bool checkpoint;             // If 'true', this instruction is a branch
                                // that needs a checkpoint.

   // Note: At present, the decode stage does not split RISCV instructions
   // into micro-instructions.  Nonetheless, the pipeline does support
   // split instructions.
   bool split;                  // Instruction is split into two micro-ops.
   bool upper;                  // If 'true': this instruction is the upper
                                // half of a split instruction.
                                // If 'false': this instruction is the lower
                                // half of a split instruction.
   bool split_store;            // Instruction is a split-store.

   // Source register A.
   bool A_valid;                // If 'true', the instruction has a
                                // first source register.
   unsigned int A_log_reg;      // The logical register specifier of the
                                // source register.

   // Source register B.
   bool B_valid;                // If 'true', the instruction has a
                                // second source register.
   unsigned int B_log_reg;      // The logical register specifier of the
                                // source register.

   // ** DESTINATION ** register C.
   bool C_valid;                // If 'true', the instruction has a
                                // destination register.
   unsigned int C_log_reg;      // The logical register specifier of the
                                // destination register.

   // ** SOURCE ** register D.
   // Floating-point multiply-accumulate uses a third source register.
   bool D_valid;                // If 'true', the instruction has a
                                // third source register.
   unsigned int D_log_reg;      // The logical register specifier of the
                                // source register.

   // IQ selection.
   sel_iq iq;                   // The value of this enumerated type indicates
                                // whether to place the instruction in the
                                // issue queue, skip it, or skip it with
                                // an exception.
                                // (The 'sel_iq' enumerated type is also
                                // defined in this file.)

   uint64_t CSR_addr;           // System register address, for privileged
                                // instructions that reference and/or modify
                                // a specified system register.

   // Details about loads and stores.
   unsigned int size;           // Size of load or store (1, 2, 4, or 8 bytes).
   bool is_signed;              // If 'true', the loaded value is signed,
                                // else it is unsigned.
   bool left;			// Relic of PISA ISA - no longer used.
   bool right;			// Relic of PISA ISA - no longer used.

   ////////////////////////
   // Set by Rename Stage.
   ////////////////////////

   // Physical registers.
   unsigned int A_phys_reg;     // If there exists a first source register (A),
                                // this is the physical register specifier to
                                // which it is renamed.
   unsigned int B_phys_reg;     // If there exists a second source register (B),
                                // this is the physical register specifier to
                                // which it is renamed.
   unsigned int C_phys_reg;     // If there exists a ** DESTINATION ** register (C),
                                // this is the physical register specifier to
                                // which it is renamed.
   unsigned int D_phys_reg;     // If there exists a third ** SOURCE ** register (D),
                                // this is the physical register specifier to
                                // which it is renamed.
  
   // Branch ID, for checkpointed branches only.
   unsigned int branch_ID;      // When a checkpoint is created for a branch,
                                // this is the branch's ID (its bit position
                                // in the Global Branch Mask).

   ////////////////////////
   // Set by Dispatch Stage.
   ////////////////////////

   unsigned int AL_index;       // Index into Active List.
   unsigned int LQ_index;       // Indices into LSU. Only used by loads, stores, and branches.
   bool LQ_phase;
   unsigned int SQ_index;
   bool SQ_phase;

   unsigned int lane_id;        // Execution lane chosen for the instruction.

   ////////////////////////
   // Set by Reg. Read Stage.
   ////////////////////////

   // Source values.
   union64_t A_value;           // If there exists a first source register (A),
                                // this is its value. To reference the value as
                                // uint64_t, use "A_value.dw".
   union64_t B_value;           // If there exists a second source register (B),
                                // this is its value. To reference the value as
                                // uint64_t, use "B_value.dw".
   union64_t D_value;           // If there exists a third ** SOURCE ** register (D),
                                // this is its value. To reference the value as
                                // uint64_t, use "D_value.dw".

   ////////////////////////
   // Set by Execute Stage.
   ////////////////////////

   // Load/store address calculated by AGEN unit.
   reg_t addr;

   // Resolved branch target. (c_next_pc: computed next program counter)
   reg_t c_next_pc;

   // Destination value.
   union64_t C_value;           // If there exists a ** DESTINATION ** register (C),
                                // this is its value. To reference the value as
                                // uint64_t, use "C_value.dw".

   uint32_t fflags;             // If it is a FP instruction, this is the new fflags bits it will post

   // If there was an exception during execution, the trap is stored here.
   trap_storage_t trap;

} payload_t;


//Forward declaring pipeline_t class as pointer is passed to the dump function
class pipeline_t;

class payload {
public:
	////////////////////////////////////////////////////////////////////////
	//
	// The new 721sim explicitly models all processor queues and
	// pipeline registers so that it is structurally the same as
	// a real pipeline. To maintain good simulation efficiency,
	// however, the simulator holds all "payload" information about
	// an instruction in a centralized data structure and only
	// pointers (indices) into this data structure are actually
	// moved through the pipeline. It is not a real hardware
	// structure but each entry collectively represents an instruction's
	// payload bits distributed throughout the pipeline.
	//
	// Each instruction is allocated two consecutive entries,
	// even and odd, in case the instruction is split into two.
	//
	////////////////////////////////////////////////////////////////////////
	payload_t    buf[PAYLOAD_BUFFER_SIZE];
	unsigned int head;
	unsigned int tail;
	int          length;

	payload();		// constructor
	unsigned int push();
	void pop();
	void clear();
	void split(unsigned int index);
	void map_to_actual(pipeline_t* proc,unsigned int index);
	void rollback(unsigned int index);
	unsigned int checkpoint();
	void restore(unsigned int index);
  void dump(pipeline_t* proc,unsigned int index, FILE* file=stderr);

	// FIX_ME: get rid of predict() function.
	// Perfect branch prediction, up to max_length instructions or the first indirect branch.
	void predict(pipeline_t *proc, uint64_t pc, uint64_t max_length, uint64_t &cb_predictions, uint64_t &indirect_target);
};

#endif //PAYLOAD_H
