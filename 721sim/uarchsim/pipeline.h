// See LICENSE for license details.
#ifndef _RISCV_PIPELINE_H
#define _RISCV_PIPELINE_H

#include "processor.h"
#include "decode.h"
#include "config.h"
#include <cstring>
#include <vector>
#include <map>
#include <cassert>

//////////////////////////////////////////////////////////////////////////////

#include "fu.h"			// function unit types

#include "parameters.h"

//////////////////////////////////////////////////////////////////////////////

#include "fetchunit_types.h"

#include "payload.h"		// instruction payload buffer

#include "pipeline_register.h"	// PIPELINE REGISTERS

#include "CacheClass.h"		// generic cache class used for instr. cache in FetchUnit, data cache in LSU, and unified L2 cache

#include "fetchunit.h"		// FETCH UNIT

#include "fetch_queue.h"	// FETCH QUEUE

#include "renamer.h"		// REGISTER RENAMER + REGISTER FILE

#include "lane.h"		// EXECUTION LANES

#include "issue_queue.h"	// ISSUE QUEUE

#include "lsu.h"		// LOAD/STORE UNIT

#include "debug.h"

#include "stats.h"

#include "alu_ops.h"

//////////////////////////////////////////////////////////////////////////////

/* instruction flags */
#define F_ICOMP		0x00000001	/* integer computation */
#define F_FCOMP		0x00000002	/* FP computation */
#define F_CTRL		0x00000004	/* control inst */
#define F_UNCOND	0x00000008	/* unconditional change */
#define F_COND		0x00000010	/* conditional change */
#define F_MEM		  0x00000020	/* memory access inst */
#define F_LOAD		0x00000040	/* load inst */
#define F_STORE		0x00000080	/* store inst */
#define F_DISP		0x00000100	/* displaced (R+C) addressing mode */
#define F_RR		  0x00000200	/* R+R addressing mode */
#define F_DIRECT	0x00000400	/* direct addressing mode */
#define F_TRAP		0x00000800	/* traping inst */
#define F_LONGLAT	0x00001000	/* long latency inst (for sched) */
#define F_FMEM	  0x00002000	/* FP memory access inst */
#define F_AMO 	  0x00004000	/* Atomic memory access inst */
#define F_CSR 	  0x00008000	/* System control instructions*/

//////////////////////////////////////////////////////////////////////////////

#define IS_BRANCH(flags)	((flags) & (F_CTRL))
#define IS_COND_BRANCH(flags)	  ((flags) & (F_COND))
#define IS_UNCOND_BRANCH(flags)	((flags) & (F_UNCOND))
#define IS_LOAD(flags)    ((flags) & (F_LOAD))
#define IS_STORE(flags)   ((flags) & (F_STORE))
#define IS_MEM_OP(flags)  ((flags) & (F_MEM))
#define IS_FP_OP(flags)   ((flags) & (F_FCOMP | F_FMEM))
#define IS_AMO(flags)     ((flags) & (F_AMO))
#define IS_CSR(flags)     ((flags) & (F_CSR))

//////////////////////////////////////////////////////////////////////////////

#define BIT_IS_ZERO(x,i)	(((x) & (((unsigned long long)1) << i)) == 0)
#define BIT_IS_ONE(x,i)		(((x) & (((unsigned long long)1) << i)) != 0)
#define SET_BIT(x,i)		  (x |= (((unsigned long long)1) << i))
#define CLEAR_BIT(x,i)		(x &= ~(((unsigned long long)1) << i))

//////////////////////////////////////////////////////////////////////////////

#define SOURCE1(in)		(in.rs1())
#define SOURCE2(in)		(in.rs2())
#define SOURCE3(in)		(in.rs3())
#define DEST(in) 		  (in.rd())

// For FP logical registers use registers 32-63.
#define FP_SOURCE1(in)		(in.rs1()+NXPR)
#define FP_SOURCE2(in)		(in.rs2()+NXPR)
#define FP_SOURCE3(in)		(in.rs3()+NXPR)
#define FP_DEST(in) 		  (in.rd() +NXPR)

//class mmu_t;
//class sim_t;
//class trap_t;
//class extension_t;
//class disassembler_t;
//
//struct insn_desc_t
//{
//	uint32_t match;
//	uint32_t mask;
//	insn_func_t rv32;
//	insn_func_t rv64;
//};
//
//struct commit_log_reg_t
//{
//	reg_t addr;
//	reg_t data;
//};
//
//// architectural state of a RISC-V hart
//struct state_t
//{
//	void reset();
//
//	reg_t pc;
//	regfile_t<reg_t, NXPR, true> XPR;
//	regfile_t<freg_t, NFPR, false> FPR;
//
//	// control and status registers
//	reg_t epc;
//	reg_t badvaddr;
//	reg_t evec;
//	reg_t ptbr;
//	reg_t pcr_k0;
//	reg_t pcr_k1;
//	reg_t cause;
//	reg_t tohost;
//	reg_t fromhost;
//	reg_t count;
//	uint32_t compare;
//	uint32_t sr; // only modify the status register using set_pcr()
//	uint32_t fflags;
//	uint32_t frm;
//
//	reg_t load_reservation;
//
//#ifdef RISCV_ENABLE_COMMITLOG
//	commit_log_reg_t log_reg_write;
//#endif
//};


// this class represents one processor in a RISC-V machine.
class pipeline_t: public processor_t
{
public:
	pipeline_t(
	    sim_t*    _sim,
	    mmu_t*    _mmu,
	    uint32_t  _id,
	    uint32_t  fq_size,
	    uint32_t  num_chkpts,
	    uint32_t  rob_size,
	    uint32_t  iq_size,
	    uint32_t  iq_num_parts,
	    uint32_t  lq_size,
	    uint32_t  sq_size,
	    uint32_t  fetch_width,
	    uint32_t  dispatch_width,
	    uint32_t  issue_width,
	    uint32_t  retire_width,
	    uint32_t  fu_lane_matrix[],
	    uint32_t  fu_lat[]
	);

	~pipeline_t();

//	void set_debug(bool value);
//	void set_histogram(bool value);
	bool get_histogram(){return histogram_enabled;}
//	void reset(bool value);
	bool step_micro(size_t instret_limit, size_t& instret); // Step the pipeline 1 cycle.
//	void deliver_ipi(); // register an interprocessor interrupt
//	bool running() {
//		return run;
//	}
//	void set_pcr(int which, reg_t val);
//	void set_fromhost(reg_t val);
//	void set_interrupt(int which, bool on);
//	reg_t get_pcr(int which);
//	mmu_t* get_mmu() {
//		return mmu;
//	}
//	state_t* get_state() {
//		return &state;
//	}
//	extension_t* get_extension() {
//		return ext;
//	}
//	void yield_load_reservation() {
//		state.load_reservation = (reg_t)-1;
//	}
	virtual void update_histogram(size_t pc);
//
//	void register_insn(insn_desc_t);
//	void register_extension(extension_t*);
//
	virtual void disasm(insn_t insn); // disassemble and print an instruction
  virtual void disasm(insn_t insn,reg_t pc); // disassemble and print an instruction
	void disasm(insn_t insn, reg_t pc, FILE* out=stderr); // disassemble and print an instruction
	void disasm(insn_t insn, cycle_t cycle, reg_t pc, uint64_t seq, FILE* out=stderr); // disassemble and print an instruction

  // Copy registers from fast skip state to pipeline register file.
  // Also reset the AMT.
  void copy_state_to_micro();
  uint64_t get_arch_reg_value(int reg_id); 
  uint64_t get_pc(){return get_state()->pc;}
  uint32_t get_instruction(uint64_t inst_pc);

private:
//	sim_t* sim;
//	mmu_t* mmu; // main memory is always accessed via the mmu
//	extension_t* ext;
//	disassembler_t* disassembler;
//	state_t state;
//	uint32_t id;
//	bool run; // !reset
//	bool debug;
//	bool histogram_enabled;
//	bool rv64;
//	bool serialized;
//
//	std::vector<insn_desc_t> instructions;
//	std::vector<insn_desc_t*> opcode_map;
//	std::vector<insn_desc_t> opcode_store;
//	std::map<size_t,size_t> pc_histogram;
//
//	void take_interrupt(); // take a trap if any interrupts are pending
//	void serialize(); // collapse into defined architectural state
	virtual reg_t take_trap(trap_t& t, reg_t epc); // take an exception
//
	friend class sim_t;
	friend class mmu_t;
	friend class extension_t;

  friend class fetch_queue;
  //friend class renamer;
  friend class payload;
  friend class issue_queue;
  friend class lsu;
  friend class CacheClass;


	//void build_opcode_map();
	//insn_func_t decode_insn(insn_t insn);




	// Timing simulator stuff below

private:
  alu_ops_t alu_ops;

  /////////////////////////////////////////////////////////////
  // Log Files
  /////////////////////////////////////////////////////////////


  FILE* fetch_log;
  FILE* decode_log;
  FILE* rename_log;
  FILE* dispatch_log;
  FILE* issue_log;
  FILE* regread_log;
  FILE* execute_log;
  FILE* lsu_log;
  FILE* wback_log;
  FILE* retire_log;
  FILE* program_log;
  FILE* stats_log;
  FILE* phase_log;
  FILE* cache_log;

  uint64_t sequence;

  /////////////////////////////////////////////////////////////
  // Statistics unit
  /////////////////////////////////////////////////////////////
  stats_t   statsModule;
  stats_t*  stats;  //Pointer to the statsModule required by macros

	/////////////////////////////////////////////////////////////
	// Instruction payload buffer.
	/////////////////////////////////////////////////////////////
	payload	PAY;

	/////////////////////////////////////////////////////////////
	// Pipeline widths.
	/////////////////////////////////////////////////////////////
	unsigned int fetch_width;	// fetch, decode width
	unsigned int dispatch_width;	// rename, dispatch width
	unsigned int issue_width;	// issue width
	unsigned int retire_width;	// retire width

	/////////////////////////////////////////////////////////////
	// Fetch unit.
	/////////////////////////////////////////////////////////////
	fetchunit_t* FetchUnit;

	/////////////////////////////////////////////////////////////
	// Pipeline register between the Fetch and Decode Stages.
	/////////////////////////////////////////////////////////////
	pipeline_register* DECODE;

	/////////////////////////////////////////////////////////////
	// Fetch Queue between the Decode and Rename Stages.
	/////////////////////////////////////////////////////////////
	fetch_queue FQ;

	/////////////////////////////////////////////////////////////
	// Pipeline register between the Rename1 and Rename2
	// sub-stages (within the Rename Stage).
	/////////////////////////////////////////////////////////////
	pipeline_register* RENAME2;

	/////////////////////////////////////////////////////////////
	// Register renaming modules.
	/////////////////////////////////////////////////////////////
	renamer* REN;

	/////////////////////////////////////////////////////////////
	// Pipeline register between the Rename and Dispatch Stages.
	/////////////////////////////////////////////////////////////
	pipeline_register* DISPATCH;

	/////////////////////////////////////////////////////////////
	// Issue Queues.
	/////////////////////////////////////////////////////////////
	issue_queue IQ;

	/////////////////////////////////////////////////////////////
	// Execution Lanes.
	/////////////////////////////////////////////////////////////
	lane* Execution_Lanes;
	unsigned int fu_lane_matrix[(unsigned int)NUMBER_FU_TYPES];	// Indexed by FU type: bit vector indicating which lanes have that FU type.
	unsigned int fu_lane_ptr[(unsigned int)NUMBER_FU_TYPES];	// Indexed by FU type: lane to which the last instruction of that FU type was steered.

	/////////////////////////////////////////////////////////////
	// Load and Store Unit.
	/////////////////////////////////////////////////////////////
	lsu LSU;

	/////////////////////////////////////////////////////////////
	// Unified L2 cache.
	/////////////////////////////////////////////////////////////
	CacheClass* L2C;

	/////////////////////////////////////////////////////////////
	// Sticky-bit memory dependence predictor (MDP)
	/////////////////////////////////////////////////////////////
	std::map<uint64_t, bool> MDP;

	//////////////////////
	// PRIVATE FUNCTIONS
	//////////////////////

	unsigned int steer(fu_type fu);
	void agen(unsigned int index);
	void alu(unsigned int index);
	void squash_complete(reg_t jump_PC);
	void resolve(unsigned int branch_ID, bool correct);
	void checker();
	void check_single(reg_t micro, reg_t isa, db_t* actual, const char *desc);
	void check_double(reg_t micro0, reg_t micro1, reg_t isa0, reg_t isa1, const char *desc);
  void check_state(state_t* micro_state, state_t* isa_state, db_t* actual);

  void phase_stats();

  bool execute_amo();
  bool execute_csr();

public:

	// The thread id.
	unsigned int Tid;

	// The simulator cycle.
	cycle_t cycle;

	// Number of instructions retired.
	uint64_t num_insn;
	uint64_t num_insn_split;
	// Functions for pipeline stages.
	void fetch();
	void decode();
	void rename1();
	void rename2();
	void dispatch();
	void schedule();
	void register_read(unsigned int lane_number);
	void execute(unsigned int lane_number);
	void writeback(unsigned int lane_number);
	void retire(size_t& instret);
	void load_replay();
	void set_exception(unsigned int al_index);
	void set_load_violation(unsigned int al_index);
	void set_branch_misprediction(unsigned int al_index);
	void set_value_misprediction(unsigned int al_index);

  //TODO: Implement these functions
	// Miscellaneous other functions.
	//void next_cycle();
 	//void stats(FILE* fp);
	//void skip(unsigned int skip_amt);
  //

  stats_t* get_stats(){return &statsModule;}
};

//reg_t illegal_instruction(processor_t* p, insn_t insn, reg_t pc);


#endif //RISCV_PIPELINE_H
