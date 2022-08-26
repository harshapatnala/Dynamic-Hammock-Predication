#include <cassert>
#include "debug.h"
#include "sim.h"
//#include "processor.h"
#include "pipeline.h"
extern bool logging_on;

// Checks to see if index 'e' lies between 'head' and 'tail'.
bool debug_buffer_t::is_active(unsigned int e) {
   if (length > 0) {
      if (head <= tail)
         return((e >= head) && (e <= tail));
      else
         return((e >= head) || (e <= tail));
   }
   else {
      return(false);
   }
}


debug_buffer_t::debug_buffer_t(unsigned int window_size) {
   // Set the full size and active size of the debug buffer.
   // Both had better be a power of two.
   // DEBUG_SIZE = 4*window_size;
   // ACTIVE_SIZE = 2*window_size;
   DEBUG_SIZE   = window_size; 
   ACTIVE_SIZE  = window_size;
   assert(IsPow2(DEBUG_SIZE) && IsPow2(ACTIVE_SIZE));

   // Allocate debug buffer.
   db = new db_t[DEBUG_SIZE];

   for(unsigned int i=0;i<DEBUG_SIZE;i++){
     db[i].entry_id = i;
     db[i].a_state = new state_t;
     db[i].a_state->reset();
   }

   // Initialize debug buffer.
   head = 0;
   tail = (DEBUG_SIZE - 1);
   length = 0;

   pc_ptr = 0;
   inst_sequence = 0;
}

debug_buffer_t::~debug_buffer_t() {
}

void debug_buffer_t::run_ahead(){
  fprintf(stderr, "Functional simulator running ahead\n");
  // Set to debug mode so that simulator single steps
  isa_sim->set_procs_debug(true);
  // Set to checker mode so that instructions are pushed to 
  // debug buffer
  isa_sim->set_procs_checker(true);
  while(hungry()){
    ifprintf(logging_on,stderr, "Functional simulator hungry\n");
    isa_sim->step();  // Step 1 cycle, which is 1 instruction for isa_sim.
  }
}

void debug_buffer_t::skip_till_pc(reg_t pc, unsigned int proc_id){
  ifprintf(logging_on,stderr, "Functional simulator skipping till PC %" PRIreg "\n",pc);
  bool old_debug = isa_sim->get_procs_debug();
  // Set to debug mode so that simulator single steps
  // and it is possible to break out of simulation
  // when the target PC is reached.
  isa_sim->set_procs_debug(true);
  isa_sim->step_till_pc(pc,proc_id);
  isa_sim->set_procs_debug(old_debug);
}

void debug_buffer_t::start() {
   // Check for overflow and maintain 'length'.
   assert(length < ACTIVE_SIZE);
   length += 1;

   // Initialize a new debug entry.
   tail = MOD((tail + 1), DEBUG_SIZE);

   assert(db[tail].entry_id == tail);

   db[tail].a_valid = true;
   db[tail].a_exception   = false;
   db[tail].a_num_rdst = 0;
   db[tail].a_num_rsrc = 0;
   db[tail].a_num_rsrcA = 0;
   db[tail].a_sequence = ++inst_sequence;

   for(unsigned int i=0;i<D_MAX_RSRC;i++)
     db[tail].a_rsrc[i].valid = 0;

   for(unsigned int i=0;i<D_MAX_RDST;i++)
     db[tail].a_rdst[i].valid = 0;

   ifprintf(logging_on,stderr, "Starting debug buffer entry %u\n",tail);
}

void debug_buffer_t::push_operand_actual( unsigned int n, operand_t t, reg_t value, reg_t pc) 
{
                          
    unsigned int i;

    ifprintf(logging_on,stderr,"Pushing operand type: %u to entry %u\n",t,tail);                          
   switch (t) {
      case RDST_OPERAND:
        // Push RS1 if not already pushed
        if(!db[tail].a_rdst[0].valid){
	        assert(db[tail].a_num_rdst < D_MAX_RDST);
	        db[tail].a_rdst[0].n = n;
	        db[tail].a_rdst[0].value = value;
	        db[tail].a_rdst[0].valid = true;
	        db[tail].a_num_rdst += 1;
        }
	      break;
      // Ordering of operand read ISA sim is not fixed
      // Hence separate operand types for RS1
      case RSRC1_OPERAND:
        // Push RS1 if not already pushed
        if(!db[tail].a_rsrc[0].valid){
	        assert(db[tail].a_num_rsrc < D_MAX_RSRC);
	        db[tail].a_rsrc[0].n = n;
	        db[tail].a_rsrc[0].value = value;
	        db[tail].a_rsrc[0].valid = true;
	        db[tail].a_num_rsrc += 1;
        }
	      break;
      // Ordering of operand read ISA sim is not fixed
      // Hence separate operand types for RS2
      case RSRC2_OPERAND:
        // Push RS2 if not already pushed
        if(!db[tail].a_rsrc[1].valid){
	        assert(db[tail].a_num_rsrc < D_MAX_RSRC);
	        db[tail].a_rsrc[1].n = n;
	        db[tail].a_rsrc[1].value = value;
	        db[tail].a_rsrc[1].valid = true;
	        db[tail].a_num_rsrc += 1;
        }
	      break;
      // Ordering of operand read ISA sim is not fixed
      // Hence separate operand types for RS3
      case RSRC3_OPERAND:
        // Push RS2 if not already pushed
        if(!db[tail].a_rsrc[2].valid){
	        assert(db[tail].a_num_rsrc < D_MAX_RSRC);
	        db[tail].a_rsrc[2].n = n;
	        db[tail].a_rsrc[2].value = value;
	        db[tail].a_rsrc[2].valid = true;
	        db[tail].a_num_rsrc += 1;
        }
	      break;
      case RSRC_A_OPERAND:
        assert(0);
	      i = db[tail].a_num_rsrcA;
	      assert(i < D_MAX_RSRC);
	      db[tail].a_rsrcA[i].n = n;
	      db[tail].a_rsrcA[i].value = value;
	      db[tail].a_rsrcA[i].valid = true;
	      db[tail].a_num_rsrcA += 1;
         break;
      default:
	      assert(0);
	      break;
   }
}

void debug_buffer_t::push_address_actual( reg_t addr,
			                    operand_t t,
			                    reg_t pc,
			                    reg_t real_upper,
			                    unsigned int real_lower) {

   assert(t == MSRC_OPERAND || t == MDST_OPERAND);
   ifprintf(logging_on,stderr,"Pushing operand type: %u to entry %u addr %lu\n",t,tail,addr);                          
   db[tail].a_addr = addr;

   db[tail].real_upper = real_upper;
   db[tail].real_lower = real_lower;
}

void debug_buffer_t::push_store_data_actual( reg_t addr,
			                    operand_t t,
			                    reg_t pc,
			                    reg_t real_upper,
			                    unsigned int real_lower) {

   assert(t == MDST_OPERAND);

   db[tail].store_data.dword = real_upper;
}

void debug_buffer_t::push_load_data_actual( reg_t addr,
			                    operand_t t,
			                    reg_t pc,
			                    reg_t real_upper,
			                    unsigned int real_lower) {

   assert(t == MSRC_OPERAND);

   db[tail].real_upper = real_upper;
}

void debug_buffer_t::push_instr_actual( insn_t       inst,
		                    unsigned int flags,
		                    unsigned int latency,
		                    reg_t        pc,
		                    reg_t        next_pc,
		                    reg_t real_upper,
		                    unsigned int real_lower) {

   db[tail].a_inst = inst;
   db[tail].a_flags = flags;
   db[tail].a_lat = latency;
   db[tail].a_pc = pc;
   db[tail].a_next_pc = next_pc;

}

void debug_buffer_t::push_exception_actual(reg_t handler_pc){
   db[tail].a_exception   = true;
   db[tail].a_next_pc     = handler_pc;
}

void debug_buffer_t::push_state_actual(state_t* a_state_ptr,bool checkpoint_state){

  // Checkpoint the entire system state
  if(checkpoint_state){

    for(size_t i = 0;i < NXPR;i++){
      db[tail].a_state->XPR.write(i,a_state_ptr->XPR[i]);
    }
    for(size_t i = 0;i < NFPR;i++){
      db[tail].a_state->FPR.write(i,a_state_ptr->FPR[i]);
    }

    db[tail].a_state->epc               =  a_state_ptr->epc;
    db[tail].a_state->badvaddr          =  a_state_ptr->badvaddr;
    db[tail].a_state->evec              =  a_state_ptr->evec;
    db[tail].a_state->ptbr              =  a_state_ptr->ptbr;
    db[tail].a_state->pcr_k0            =  a_state_ptr->pcr_k0;
    db[tail].a_state->pcr_k1            =  a_state_ptr->pcr_k1;
    db[tail].a_state->cause             =  a_state_ptr->cause;
    db[tail].a_state->tohost            =  a_state_ptr->tohost;
    db[tail].a_state->fromhost          =  a_state_ptr->fromhost;
    db[tail].a_state->count             =  a_state_ptr->count;
    db[tail].a_state->compare           =  a_state_ptr->compare;
    db[tail].a_state->sr                =  a_state_ptr->sr;
    db[tail].a_state->fflags            =  a_state_ptr->fflags;
    db[tail].a_state->frm               =  a_state_ptr->frm;
    db[tail].a_state->load_reservation  =  a_state_ptr->load_reservation;

  } 
  // Checkpoint only state necessary for checking
  else 
  {

    //db[tail].a_state->epc               =  a_state_ptr->epc;
    db[tail].a_state->badvaddr          =  a_state_ptr->badvaddr;
    //db[tail].a_state->evec              =  a_state_ptr->evec;
    //db[tail].a_state->ptbr              =  a_state_ptr->ptbr;
    //db[tail].a_state->pcr_k0            =  a_state_ptr->pcr_k0;
    //db[tail].a_state->pcr_k1            =  a_state_ptr->pcr_k1;
    //db[tail].a_state->cause             =  a_state_ptr->cause;
    db[tail].a_state->tohost            =  a_state_ptr->tohost;
    db[tail].a_state->fromhost          =  a_state_ptr->fromhost;
    db[tail].a_state->count             =  a_state_ptr->count;
    //db[tail].a_state->compare           =  a_state_ptr->compare;
    db[tail].a_state->sr                =  a_state_ptr->sr;
    db[tail].a_state->fflags            =  a_state_ptr->fflags;
    db[tail].a_state->frm               =  a_state_ptr->frm;
    //db[tail].a_state->load_reservation  =  a_state_ptr->load_reservation;
  }
}

// Assert that the index of the head debug buffer entry equals 'i'.
// Then pop the head entry, returning a pointer to the contents of
// that entry.
db_t* debug_buffer_t::pop(debug_index_t i) {

   ifprintf(logging_on,stderr, "Timing simulator popping entry %u\n",i);
   assert(i == head);

   // Set the valid bit to 0 so that perfect branch prediction
   // does not fail at PC mismatch assertion at the end of the 
   // program.
   db[head].a_valid = false;


   // Fill out the debug buffer
   // Make sure the simulator is still running and is not already 
   // done with the program.
   while(hungry() && isa_sim->running()){
    ifprintf(logging_on,stderr, "Functional simulator hungry\n");
     isa_sim->step();  // Step 1 cycle, which is 1 instruction for isa_sim.
   }

   // Check for underflow and maintain 'length'.
   assert(length > 0);
   length -= 1;

   // Pop the head entry by advancing head pointer.
   head = MOD((head + 1), DEBUG_SIZE);


   // Return a pointer to (what was) the head entry.
   return( &(db[i]) );
}



// Check if the entry following 'i' has the same
// program counter value as 'pc'.
// If yes, then return the index of the entry following 'i',
// else return DEBUG_INDEX_INVALID.
debug_index_t debug_buffer_t::check_next(debug_index_t i, reg_t pc) {

   unsigned int e;

   // get the next entry
   e = MOD((i + 1), DEBUG_SIZE);

   if (is_active(e) && (pc == db[e].a_pc))
      return(e);
   else
      return(DEBUG_INDEX_INVALID);
}

// Search for the first occurrence of 'pc' in the debug buffer,
// starting with the entry _after_ 'i'.
// If found, return that entry's index, else return DEBUG_INDEX_INVALID.
debug_index_t debug_buffer_t::find(debug_index_t i, reg_t pc) {

   unsigned int e;

   // Get the next entry.
   e = MOD((i + 1), DEBUG_SIZE);

   // While within the active region of debug buffer,
   // search for 'pc'.
   while (is_active(e)) {
      if (pc == db[e].a_pc)
        return(e);			// FOUND IT!
      else
	      e = MOD((e + 1), DEBUG_SIZE);
   }

   // Must not have found it if loop takes normal exit.
   return(DEBUG_INDEX_INVALID);
}

void debug_buffer_t::dump(pipeline_t* proc, db_t* actual, FILE* file)
{
  proc->disasm(actual->a_inst,proc->cycle,actual->a_pc,actual->a_sequence,file);
  ifprintf(logging_on,file,"next_pc    : %" PRIxreg "\t",  actual->a_next_pc);
  ifprintf(logging_on,file,"Mem addr   : %" PRIxreg "\t",  actual->a_addr);
  ifprintf(logging_on,file,"entry_id   : %" PRIu64  "\t",  actual->entry_id);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"RS1 Valid  : %u\t",            (actual->a_num_rsrc > 0));
  ifprintf(logging_on,file,"RS1 Logical: %u\t",            actual->a_rsrc[0].n);
  ifprintf(logging_on,file,"RS1 Value  : 0x%" PRIxreg "\t",actual->a_rsrc[0].value);
  ifprintf(logging_on,file,"RS1 Value  : %" PRIsreg "\t",  actual->a_rsrc[0].value);
  ifprintf(logging_on,file,"RS1 Value  : %f\t",            (double)actual->a_rsrc[0].value);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"RS2 Valid  : %u\t",            (actual->a_num_rsrc > 1));
  ifprintf(logging_on,file,"RS2 Logical: %u\t",            actual->a_rsrc[1].n);
  ifprintf(logging_on,file,"RS2 Value  : 0x%" PRIxreg "\t",actual->a_rsrc[1].value);
  ifprintf(logging_on,file,"RS2 Value  : %" PRIsreg "\t",  actual->a_rsrc[1].value);
  ifprintf(logging_on,file,"RS2 Value  : %f\t",            (double)actual->a_rsrc[1].value);
  ifprintf(logging_on,file,"\n");
  if(unlikely(actual->a_num_rsrc > 2)){
    ifprintf(logging_on,file,"RS3 Valid  : %u\t",            (actual->a_num_rsrc > 2));
    ifprintf(logging_on,file,"RS3 Logical: %u\t",            actual->a_rsrc[2].n);
    ifprintf(logging_on,file,"RS3 Value  : 0x%" PRIxreg "\t",actual->a_rsrc[2].value);
    ifprintf(logging_on,file,"RS3 Value  : %" PRIsreg "\t",  actual->a_rsrc[2].value);
    ifprintf(logging_on,file,"RS1 Value  : %f\t",            (double)actual->a_rsrc[2].value);
    ifprintf(logging_on,file,"\n");
  }
  ifprintf(logging_on,file,"RD  Valid  : %u\t",            (actual->a_num_rdst > 0));
  ifprintf(logging_on,file,"RD  Logical: %u\t",            actual->a_rdst[0].n);
  ifprintf(logging_on,file,"RD  Value  : 0x%" PRIxreg "\t",actual->a_rdst[0].value);
  ifprintf(logging_on,file,"RD  Value  : %" PRIsreg "\t",  actual->a_rdst[0].value);
  ifprintf(logging_on,file,"RD  Value  : %f\t",            (double)actual->a_rdst[0].value);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"\n");
} 

