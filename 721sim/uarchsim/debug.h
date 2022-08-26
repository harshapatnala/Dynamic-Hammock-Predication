#ifndef DEBUG_H
#define DEBUG_H

#include <cstdio>
#include <cassert>
#include "common.h"
#include "decode.h"


struct state_t;

///////////////
// DEFINES
///////////////

#define	D_MAX_RDST	1
#define	D_MAX_RSRC	3

#define DEBUG_INDEX_INVALID	0xBADBEEF

#define valid_debug_index(x) ((size_t)x != (size_t)DEBUG_INDEX_INVALID)

///////////////
// TYPES
///////////////

//typedef enum {
//  RSRC_OPERAND,
//  RDST_OPERAND,
//  MSRC_OPERAND,
//  MDST_OPERAND,
//  RSRC_A_OPERAND
//} operand_t;

typedef struct {
	unsigned int  n;		 // arch register
	reg_t         value; // destination value
  bool          valid; // Already pushed
} db_reg_t;

typedef union {
  reg_t dword; //Double word
  sreg_t sdword; //Signed double word
	word_t words[2];
	unsigned char bytes[8];
} store_data_t;

typedef struct {

  uint64_t      entry_id;
  bool          a_valid;

	// state from functional simulator
	reg_t	        a_pc;
	reg_t	        a_next_pc;
	insn_t	      a_inst;
  uint64_t      a_sequence;

  bool          a_exception;
	unsigned int	a_flags;
	unsigned int	a_lat;
	unsigned int	a_num_rdst;
	db_reg_t	    a_rdst[D_MAX_RDST];
	unsigned int	a_num_rsrc;
	db_reg_t	    a_rsrc[D_MAX_RSRC];
	unsigned int	a_num_rsrcA;
	db_reg_t	    a_rsrcA[D_MAX_RSRC];
	reg_t       	a_addr;

	// aligned doubleword of memory data
	reg_t	        real_upper;
	unsigned int	real_lower;

	// ER: 11/6/99
	// For stores, real_upper/real_lower is the doubleword of memory
	// before the store is performed.  The following data structure
	// contains the doubleword *after* the store is performed,
	// and can be accessed either as words or individual bytes.
	store_data_t    store_data;

  state_t*  a_state;

	// STATS
	unsigned int why_vector;
} db_t;

typedef unsigned int	debug_index_t;

class sim_t;
class pipeline_t;

class debug_buffer_t {

private:
	///////////////////////////////////////////////////
	// DEBUG BUFFER
	///////////////////////////////////////////////////

	unsigned int DEBUG_SIZE;
	unsigned int ACTIVE_SIZE;

	db_t* db;
	debug_index_t head;
	debug_index_t tail;
	unsigned int  length;

  uint64_t    inst_sequence;

  //TODO: Check the type of this
	debug_index_t pc_ptr;	// used by pop_pc()

  sim_t* isa_sim;

  ///////////////////////
  // PRIVATE FUNCTIONS
  ///////////////////////

  // Checks to see if index 'e' lies between 'head' and 'tail'.
  bool is_active(unsigned int e);

public:
	///////////////
	// INTERFACE
	///////////////

	debug_buffer_t(unsigned int window_size);
	~debug_buffer_t();

  void set_isa_sim(sim_t* _isa_sim){ isa_sim = _isa_sim; }
  void run_ahead();
  void skip_till_pc(reg_t pc, unsigned int proc_id);

	//////////////////////////////////////////////////////////////
	// Interface for collecting functional simulator state.
	//////////////////////////////////////////////////////////////

	inline	bool hungry() {
    ifprintf(logging_on,stderr, "Debug buffer head: %u tail %u length %u active size %u\n",head,tail,length,ACTIVE_SIZE);
	   return(length < ACTIVE_SIZE);
	}

	void start();
	void push_operand_actual( unsigned int n, operand_t t, reg_t value, reg_t pc);
	void push_address_actual( reg_t addr, operand_t t, reg_t pc, reg_t real_upper, unsigned int real_lower);
	void push_load_data_actual( reg_t addr, operand_t t, reg_t pc, reg_t real_upper, unsigned int real_lower);
	void push_store_data_actual( reg_t addr, operand_t t, reg_t pc, reg_t real_upper, unsigned int real_lower);
	void push_instr_actual( insn_t inst, unsigned int flags, unsigned int latency, reg_t pc, reg_t next_pc, reg_t real_upper, unsigned int real_lower);
  void push_exception_actual(reg_t handler_pc);
  void push_state_actual(state_t* a_state_ptr, bool checkpoint_state);
	//////////////////////////////////////////////////////////////
	// Interface for mapping ROB entries to debug buffer entries.
	//////////////////////////////////////////////////////////////

	// Assert that the head debug buffer entry has a program counter
	// value equal to 'pc'.
	// Then return the index of the head entry.
	inline debug_index_t first(reg_t pc) {
		//HP--printf("In Debug.h First PC in DBUF - %x\n", db[head].a_pc);
	   assert(pc == db[head].a_pc);
	   return(head);
	}

	// Check if the entry following 'i' has the same
	// program counter value as 'pc'.
	// If yes, then return the index of the entry following 'i',
	// else return DEBUG_INDEX_INVALID.
	debug_index_t check_next(debug_index_t i, reg_t pc);

	// Search for the first occurrence of 'pc' in the debug buffer,
	// starting with the entry _after_ 'i'.
	// If found, return that entry's index, else return DEBUG_INDEX_INVALID.
	debug_index_t find(debug_index_t i, reg_t pc);


	//////////////////////////////////////////////////////////////
	// Interface for viewing debug buffer state.
	//////////////////////////////////////////////////////////////

	// Return a pointer to the contents of an arbitrary debug buffer entry.
	// The debug buffer entry must be in the 'active window' of the buffer.
	inline	db_t *peek(debug_index_t i) {
	   assert(is_active(i));
	   return( &(db[i]) );
	}

	inline	bool empty() {
	   return(length == 0);
	}

  db_t* pop(debug_index_t i);

	//////////////////////////////////////////////////////////////
	// Interface for facilitating perfect branch prediction.
	//////////////////////////////////////////////////////////////

	inline	reg_t pop_pc() {
	   // Return PC of *next* instruction.
	   pc_ptr = MOD((pc_ptr + 1), DEBUG_SIZE);
	   return(db[pc_ptr].a_pc);
	}

	inline	bool pop_pc_valid() {
	   // Return PC valid of *next* instruction.
	   unsigned int ptr = MOD((pc_ptr + 1), DEBUG_SIZE);
	   return(db[ptr].a_valid);
	}

	inline	void recover_pc_ptr(reg_t recover_PC) {
	   pc_ptr = head;
     //unsigned int next_pc_ptr = pc_ptr = MOD((pc_ptr + 1), DEBUG_SIZE);
     //assert(recover_PC == db[pc_ptr].a_pc);
	}

	inline	void recover_pc_ptr(reg_t recover_PC, unsigned int index) {
	   pc_ptr = MOD((index + 1), DEBUG_SIZE);
     //unsigned int next_pc_ptr = pc_ptr = MOD((pc_ptr + 1), DEBUG_SIZE);
     //assert(recover_PC == db[pc_ptr].a_pc);
	}

	///////////////////////////////////////////////////////////////////////
	// Pushing of the last instruction (syscall-exit) onto the debug buffer
	// did not complete, because we appropriately exited func_sim() upon
	// executing the syscall!  So, complete the process.
	///////////////////////////////////////////////////////////////////////

	inline	void finish_syscall_exit() {
	   unsigned int x;
	   insn_t inst;

	   x = MOD((tail + DEBUG_SIZE - 1), DEBUG_SIZE);
     //TODO:Set correct value
	   //inst.a = 0x6f;
	   //inst.b = 0x0;

     //TODO: Uncomment this and chek functionality
	   //push_instr_actual(inst, F_TRAP, 1, db[x].a_next_pc, 0, 0, 0);
	}


  void dump(pipeline_t* proc, db_t* actual, FILE* file); 
};


#endif //DEBUG_H
