#ifndef _FETCHUNIT_TYPES_H
#define _FETCHUNIT_TYPES_H

typedef
enum {
   BTB_BRANCH,
   BTB_JUMP_DIRECT,
   BTB_CALL_DIRECT,
   BTB_JUMP_INDIRECT,
   BTB_CALL_INDIRECT,
   BTB_RETURN,
   HAMMOCK
} btb_branch_type_e;

typedef 
enum {
	NORMAL=0,
	THEN=1,
	ELSE=2,
	CMOV=3
} inst_region_e;

//------- ADDED CODE-----
typedef
enum {
	REGULAR,			//REGULAR Mode
	PREDICATED_REGION,	// Inside the hammock region
	CMOV_Region			//CMOV Injection Mode
} fetch_state_e;

typedef
struct {
	// For a trace cache fetch bundle, all fields are set by the trace cache.

	// For an instruction cache fetch bundle: these fields are set by the BTB.
	bool valid;			// If true, this slot in the fetch bundle contains a valid instruction.
	bool branch;			// This instruction was identified as a branch, by the BTB (if bundle came from instr. cache) or by the trace cache.
	btb_branch_type_e branch_type;	// If the instruction was identified as a branch, this is its type.
	uint64_t branch_target;         // If the instruction was identified as a branch, this is its taken target (not valid for indirect branches).
	uint64_t pc;			// PC of the instruction.
	uint64_t next_pc;		// PC of the next instruction fetched after this one.

	// For an instruction cache fetch bundle: these fields are set by the instruction cache.
	bool exception;			// If true, this slot in the fetch bundle caused a fetch exception.
	reg_t exception_cause;		// If exception, this is the cause.
	insn_t insn;			// The instruction.
	
	inst_region_e region_type;
	uint64_t cmov_log_reg;
	bool is_hammock;
} fetch_bundle_t;


typedef
struct {
	bool valid;			// There is a fetch bundle in the Fetch2 stage.
	uint64_t pc;			// PC of the fetch bundle.
	uint64_t cb_bhr;		// Conditional branch predictor's BHR prior to the fetch bundle.
	uint64_t ib_bhr;		// Indirect branch predictor's BHR prior to the fetch bundle.
	uint64_t ras_tos;		// TOS pointer into the RAS prior to the fetch bundle.
	uint64_t pay_checkpoint;	// Checkpoint of where PAY was at, prior to the fetch bundle.
	bool tc_hit;			// If true, the fetch bundle came from the trace cache, else it came from the instruction cache.
} fetch2_status_t;


typedef
struct {
	uint64_t next_pc;	// Predicted PC of the next fetch bundle.
	uint64_t num_cb;	// Number of conditional branches in the fetch bundle.
	bool pop_ras;		// Fetch bundle ends in a return instruction, so pop the RAS.
	bool push_ras;		// Fetch bundle ends in a call direct/indirect instruction, so push the RAS.
	uint64_t push_ras_pc;	// This is the pc to push onto the RAS if directed to do so.
	fetch_state_e state;
} spec_update_t;



#endif
