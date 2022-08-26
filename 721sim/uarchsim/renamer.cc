#include <renamer.h>
#include <stdio.h>
////////////////////////////////////////
// Public functions.
////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// This is the constructor function.
// When a renamer object is instantiated, the caller indicates:
// 1. The number of logical registers (e.g., 32).
// 2. The number of physical registers (e.g., 128).
// 3. The maximum number of unresolved branches.
//    Requirement: 1 <= n_branches <= 64.
//
// Tips:
//
// Assert the number of physical registers > number logical registers.
// Assert 1 <= n_branches <= 64.
// Then, allocate space for the primary data structures.
// Then, initialize the data structures based on the knowledge
// that the pipeline is intially empty (no in-flight instructions yet).
renamer::renamer(uint64_t n_log_regs,uint64_t n_phys_regs,uint64_t n_branches){
   assert (n_phys_regs>n_log_regs);
   assert (n_branches>=1 && n_branches<=64); 
   
   n_max_branches = n_branches; 
   n_logical_regs = n_log_regs;
   n_physical_regs = n_phys_regs;
   size_of_free_and_active_list = n_physical_regs-n_logical_regs-1;

   RMT       = new RMT_struct[n_log_regs];
   for(uint i=0;i<n_log_regs;i++){
     RMT[i].phy_reg=i;
     RMT[i].t_valid=false;
     RMT[i].e_valid=false;
   } 

   AMT       = new uint[n_log_regs];
   for(uint i=0;i<n_log_regs;i++)
   AMT[i]=i;

   AMT_64 = n_log_regs;
   RMT_64 = n_log_regs;

   FL        = new free_list_struct(n_phys_regs-n_log_regs-1);
   for(uint i=0;i<n_phys_regs-n_log_regs-1;i++){
   FL->FL_Entry[i]=i+n_log_regs+1;
   
   //printf("Fl entry %d is %d \n",i,FL->FL_Entry[i]);
   }
   
   AL        = new active_list_struct(n_phys_regs-n_log_regs); 

   PRF       = new uint64_t[n_phys_regs]; 
   for(int i=0;i<n_phys_regs;i++)
   PRF[i]=0;

   PRF_ready = new bool[n_phys_regs];

   //for(int i=0;i<n_phys_regs;i++)
   //if(i<n_log_regs) PRF_ready[i]=true; else PRF_ready[i]=false;
   for(int i=0;i<n_phys_regs;i++)
    PRF_ready[i]=true;
   
    

   GBM       = 0;

   branch_checkpoint = new  branch_checkpoint_struct[n_branches];
   for(int i=0;i<n_branches;i++)
      branch_checkpoint[i]=branch_checkpoint_struct(n_log_regs);
}
//////////////////////////////////////////
// Functions related to Rename Stage.   //
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// The Rename Stage must stall if there aren't enough free physical
// registers available for renaming all logical destination registers
// in the current rename bundle.
//
// Inputs:
// 1. bundle_dst: number of logical destination registers in
//    current rename bundle
//
// Return value:
// Return "true" (stall) if there aren't enough free physical
// registers to allocate to all of the logical destination registers
// in the current rename bundle.
/////////////////////////////////////////////////////////////////////
bool renamer::stall_reg(uint64_t bundle_dst){
   if(FL->free_space() >= bundle_dst)return false;
   else return true;

}

/////////////////////////////////////////////////////////////////////
// The Rename Stage must stall if there aren't enough free
// checkpoints for all branches in the current rename bundle.
//
// Inputs:
// 1. bundle_branch: number of branches in current rename bundle
//
// Return value:
// Return "true" (stall) if there aren't enough free checkpoints
// for all branches in the current rename bundle.
/////////////////////////////////////////////////////////////////////
bool renamer::stall_branch(uint64_t bundle_branch){
  uint64_t free_entries=0;
  uint64_t pos=0;
  uint64_t mask=1;

  while(pos<n_max_branches){
   if((~GBM & mask) == mask)
     free_entries++;
   mask = mask<<1;
   pos++;
  }

  if(free_entries>=bundle_branch) return false;
  else return true; 
  //return false;
}

/////////////////////////////////////////////////////////////////////
// This function is used to get the branch mask for an instruction.
/////////////////////////////////////////////////////////////////////
uint64_t renamer::get_branch_mask(){
  return GBM;
}

/////////////////////////////////////////////////////////////////////
// This function is used to rename a single source register.
//
// Inputs:
// 1. log_reg: the logical register to rename
//
// Return value: physical register name
/////////////////////////////////////////////////////////////////////
/*
uint64_t renamer::rename_rsrc(uint64_t log_reg){
  return RMT[log_reg].phy_reg;
}
*/

uint64_t renamer::rename_rsrc(uint64_t log_reg,instruction_dhp_e dhp_type){
  if(dhp_type == CMOV_TYPE && log_reg ==64 ) {
     return RMT_64;
  }
  if(dhp_type == NORMAL_TYPE){
    return RMT[log_reg].phy_reg;
  }  
  else if(dhp_type == THEN_TYPE){
    if(RMT[log_reg].t_valid == true) return RMT[log_reg].t_phy_reg;
    else return RMT[log_reg].phy_reg;
  }  
  else if(dhp_type == ELSE_TYPE){
    if(RMT[log_reg].e_valid == true) return RMT[log_reg].e_phy_reg;
    else return RMT[log_reg].phy_reg;
  }  
}

/////////////////////////////////////////////////////////////////////
// This function is used to rename a single destination register.
//
// Inputs:
// 1. log_reg: the logical register to rename
//
// Return value: physical register name
/////////////////////////////////////////////////////////////////////
/*
uint64_t renamer::rename_rdst(uint64_t log_reg){
 uint64_t phy_reg; 
 phy_reg = FL->FL_Entry[FL->head];
 RMT[log_reg].phy_reg= phy_reg;
 //PRF_ready[phy_reg]=false;
 //check full condition
 if(FL->empty ==1) FL->empty=0; 
 if(FL->tail-FL->head == 1 || ((FL->head == FL->size -1)&&(FL->tail==0)) ) FL->full=1; else FL->full=0; 
 if(FL->head == FL->size-1) FL->head=0; else FL->head++;
 //printf("phy_reg is %d,%d: & Fl empty is %d\n",phy_reg,FL->head,FL->empty);
 //printf("rename_rdst::FL full is %d, head is %d,tail is %d,FL->size is %d,phy_reg is %d\n",FL->full,FL->head,FL->tail,FL->free_space(),phy_reg);
 return phy_reg;
}
*/
uint64_t renamer::rename_rdst(uint64_t log_reg,instruction_dhp_e dhp_type){
 uint64_t phy_reg; 
 phy_reg = FL->FL_Entry[FL->head];
 if(dhp_type == NORMAL_TYPE || dhp_type == CMOV_TYPE){
   if(log_reg == 64 && dhp_type == NORMAL_TYPE){
     RMT_64 = phy_reg;
      //printf(" rename_dst::New RMT_value is:%d\n",RMT_64);
      for(uint i=0;i<n_logical_regs;i++){
       RMT[i].e_valid = false;
       RMT[i].t_valid = false; 
      }
   }  
   else
     RMT[log_reg].phy_reg= phy_reg;
 }
 else if(dhp_type == THEN_TYPE){
   RMT[log_reg].t_phy_reg = phy_reg;
   RMT[log_reg].t_valid = true;
 }
 else if(dhp_type == ELSE_TYPE){
   RMT[log_reg].e_phy_reg= phy_reg;
   RMT[log_reg].e_valid = true;
 }  
 //PRF_ready[phy_reg]=false;
 //check full condition
 if(FL->empty ==1) FL->empty=0; 
 if(FL->tail-FL->head == 1 || ((FL->head == FL->size -1)&&(FL->tail==0)) ) FL->full=1; else FL->full=0; 
 if(FL->head == FL->size-1) FL->head=0; else FL->head++;
 //printf("phy_reg is %d,%d: & Fl empty is %d\n",phy_reg,FL->head,FL->empty);
 //printf("rename_rdst::FL full is %d, head is %d,tail is %d,FL->size is %d,phy_reg is %d\n",FL->full,FL->head,FL->tail,FL->free_space(),phy_reg);
 return phy_reg;
}

/////////////////////////////////////////////////////////////////////
// This function creates a new branch checkpoint.
//
// Inputs: none.
//
// Output:
// 1. The function returns the branch's ID. When the branch resolves,
//    its ID is passed back to the renamer via "resolve()" below.
//
// Tips:
//
// Allocating resources for the branch (a GBM bit and a checkpoint):
// * Find a free bit -- i.e., a '0' bit -- in the GBM. Assert that
//   a free bit exists: it is the user's responsibility to avoid
//   a structural hazard by calling stall_branch() in advance.
// * Set the bit to '1' since it is now in use by the new branch.
// * The position of this bit in the GBM is the branch's ID.
// * Use the branch checkpoint that corresponds to this bit.
// 
// The branch checkpoint should contain the following:
// 1. Shadow Map Table (checkpointed Rename Map Table)
// 2. checkpointed Free List head index
// 3. checkpointed GBM
/////////////////////////////////////////////////////////////////////
uint64_t renamer::checkpoint(){
  uint64_t pos=0;
  uint64_t mask=1;
  bool found=false;
  while(pos<n_max_branches){
   if((~GBM & mask) == mask){found=true;break;}
   mask = mask<<1;
   pos++;
  }
  if(found==true) GBM = GBM| 1<<pos;
  else printf("No empty check point found\n"); 

  for(int i=0;i<n_logical_regs;i++){
    branch_checkpoint[pos].RMT[i].phy_reg=RMT[i].phy_reg;
    branch_checkpoint[pos].RMT[i].t_valid=false; 
    branch_checkpoint[pos].RMT[i].t_valid=false;
    branch_checkpoint[pos].RMT_64=RMT_64;

  }  
  
  branch_checkpoint[pos].GBM= GBM;
  branch_checkpoint[pos].freelist_head= FL->head;
  //printf("check point is %d\n",pos);
   
  return pos;
}

//////////////////////////////////////////
// Functions related to Dispatch Stage. //
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// The Dispatch Stage must stall if there are not enough free
// entries in the Active List for all instructions in the current
// dispatch bundle.
//
// Inputs:
// 1. bundle_inst: number of instructions in current dispatch bundle
//
// Return value:
// Return "true" (stall) if the Active List does not have enough
// space for all instructions in the dispatch bundle.
/////////////////////////////////////////////////////////////////////
bool renamer::stall_dispatch(uint64_t bundle_inst){
  if(AL->free_space() >=bundle_inst )return false;
  return true; 
}

/////////////////////////////////////////////////////////////////////
// This function dispatches a single instruction into the Active
// List.
//
// Inputs:
// 1. dest_valid: If 'true', the instr. has a destination register,
//    otherwise it does not. If it does not, then the log_reg and
//    phys_reg inputs should be ignored.
// 2. log_reg: Logical register number of the instruction's
//    destination.
// 3. phys_reg: Physical register number of the instruction's
//    destination.
// 4. load: If 'true', the instr. is a load, otherwise it isn't.
// 5. store: If 'true', the instr. is a store, otherwise it isn't.
// 6. branch: If 'true', the instr. is a branch, otherwise it isn't.
// 7. amo: If 'true', this is an atomic memory operation.
// 8. csr: If 'true', this is a system instruction.
// 9. PC: Program counter of the instruction.
//
// Return value:
// Return the instruction's index in the Active List.
//
// Tips:
//
// Before dispatching the instruction into the Active List, assert
// that the Active List isn't full: it is the user's responsibility
// to avoid a structural hazard by calling stall_dispatch()
// in advance.
/////////////////////////////////////////////////////////////////////
uint64_t renamer::dispatch_inst(bool dest_valid,
                       uint64_t log_reg,
                       uint64_t phys_reg,
                       bool load,
                       bool store,
                       bool branch,
                       bool amo,
                       bool csr,
                       uint64_t PC,
                       instruction_dhp_e instruction_type,
                       bool is_hammock   
                       ){
     uint64_t instr_idx;
     //printf("AL tail value is: %d\n",AL->tail);
     AL->AL_Entry[AL->tail].dest_valid      = dest_valid;
     AL->AL_Entry[AL->tail].dest_logic_reg  = log_reg;
     AL->AL_Entry[AL->tail].dest_phy_reg    = phys_reg;
     AL->AL_Entry[AL->tail].load_flag       = load;
     AL->AL_Entry[AL->tail].store_flag      = store;
     AL->AL_Entry[AL->tail].branch_flag     = branch;
     AL->AL_Entry[AL->tail].amo_flag        = amo;
     AL->AL_Entry[AL->tail].csr_flag        = csr;
     AL->AL_Entry[AL->tail].PC              = PC;
     AL->AL_Entry[AL->tail].completed       = false;
     AL->AL_Entry[AL->tail].exception       = false;
     AL->AL_Entry[AL->tail].load_violation  = false;
     AL->AL_Entry[AL->tail].branch_mispred  = false;
     AL->AL_Entry[AL->tail].value_mispred   = false;
     AL->AL_Entry[AL->tail].predication_tag = RMT_64; //phy_reg number for logical reg 64

     AL->AL_Entry[AL->tail].instruction_type= instruction_type;
     AL->AL_Entry[AL->tail].deactivated   = false;

     dispatch_cnt++;
     //if(is_hammock)  printf("dispatch::NEW RMT 64 value in dispatch: %d dispatch_cnt:%d\n", RMT_64,dispatch_cnt);
     //if(AL->AL_Entry[AL->tail].instruction_type==ELSE_TYPE)  printf("dispatch::ELSE Got RMT_64: %d dispatch_cnt:%d\n", AL->AL_Entry[AL->tail].predication_tag,dispatch_cnt);
     //if(AL->AL_Entry[AL->tail].instruction_type==CMOV_TYPE)  printf("dispatch::CMOV Got RMT_64: %d dispatch_cnt:%d\n", AL->AL_Entry[AL->tail].predication_tag,dispatch_cnt);
     
     //if(AL->AL_Entry[AL->tail].instruction_type== THEN_TYPE){
     //  printf("dispatch::THEN TYPE DISPATCH PRED_TAG %d %d dispatch_cnt:%d\t", RMT_64, AL->AL_Entry[AL->tail].predication_tag,dispatch_cnt);
     //  uint64_t temp_tail = (AL->tail == 0) ? (AL->size -1) : (AL->tail - 1);
     //  printf("dispatch::PREVIOUS INDEX Dest_Phy_reg %d\n", AL->AL_Entry[temp_tail].predication_tag);
     //}
     
      
     if(dest_valid==1) PRF_ready[phys_reg]=false;
     instr_idx =AL->tail;

     //check full condition
     if(AL->empty ==1) AL->empty=0; 
     if(AL->head-AL->tail == 1 || ((AL->tail == AL->size -1)&&(AL->head==0)) ) AL->full=1; else AL->full=0; 
     if(AL->tail == AL->size-1) AL->tail=0; else AL->tail++;
     //printf("dispatch_inst::AL full is %d, head is %d,tail is %d,AL->left size is %d\n",AL->full,AL->head,AL->tail,AL->free_space());
     //printf("dest valid=%d,phys_reg=%d\n",dest_valid,phys_reg);
     return instr_idx;
}


//////////////////////////////////////////
// Functions related to Schedule Stage. //
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// Test the ready bit of the indicated physical register.
// Returns 'true' if ready.
/////////////////////////////////////////////////////////////////////
bool renamer::is_ready(uint64_t phys_reg){
   return PRF_ready[phys_reg];
}

/////////////////////////////////////////////////////////////////////
// Clear the ready bit of the indicated physical register.
/////////////////////////////////////////////////////////////////////
void renamer::clear_ready(uint64_t phys_reg){
  PRF_ready[phys_reg]=false;
}

/////////////////////////////////////////////////////////////////////
// Set the ready bit of the indicated physical register.
/////////////////////////////////////////////////////////////////////
void renamer::set_ready(uint64_t phys_reg){
  PRF_ready[phys_reg]=true;
}


//////////////////////////////////////////
// Functions related to Reg. Read Stage.//
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// Return the contents (value) of the indicated physical register.
/////////////////////////////////////////////////////////////////////
uint64_t renamer::read(uint64_t phys_reg){
  return PRF[phys_reg];
}


//////////////////////////////////////////
// Functions related to Writeback Stage.//
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// Write a value into the indicated physical register.
/////////////////////////////////////////////////////////////////////
void renamer::write(uint64_t phys_reg, uint64_t value){
  PRF[phys_reg]= value;
}

/////////////////////////////////////////////////////////////////////
// Set the completed bit of the indicated entry in the Active List.
/////////////////////////////////////////////////////////////////////
void renamer::set_complete(uint64_t AL_index){
   AL->AL_Entry[AL_index].completed=1;
}

void renamer::predicate_done(uint64_t AL_index,uint64_t predication_tag,bool predicate_outcome){
   uint64_t search_AL_index ;
    if(AL_index == AL->size -1) search_AL_index =0 ; else search_AL_index =AL_index+1 ; // Valid Increment?
   //while( (AL->AL_Entry[search_AL_index].predication_tag == predication_tag) && AL->AL_Entry[search_AL_index].instruction_type != CMOV_TYPE){
     while(AL->AL_Entry[search_AL_index].instruction_type != CMOV_TYPE) {
     if(AL->AL_Entry[search_AL_index].instruction_type == THEN_TYPE) {
       if(predicate_outcome == 1) AL->AL_Entry[search_AL_index].deactivated = true;
       else AL->AL_Entry[search_AL_index].deactivated = false;
       //if(predication_tag != AL->AL_Entry[search_AL_index].predication_tag) printf(" AL PREDICATE TAG- %d, BROADCASTED TAG-%d--------------------------------------------------PREDICATION TAGS NOT MATCHING\n", AL->AL_Entry[search_AL_index].predication_tag, predication_tag);
     }
      else if(AL->AL_Entry[search_AL_index].instruction_type == ELSE_TYPE) {
        if(predicate_outcome == 0) AL->AL_Entry[search_AL_index].deactivated = true;
        else AL->AL_Entry[search_AL_index].deactivated = false;
         //if(predication_tag != AL->AL_Entry[search_AL_index].predication_tag) printf("--------------------------------------------------PREDICATION TAGS NOT MATCHING\n");
      }



    //  if((AL->AL_Entry[search_AL_index].instruction_type == THEN_TYPE && predicate_outcome==1 )||(AL->AL_Entry[search_AL_index].instruction_type == ELSE_TYPE && predicate_outcome==0 )) 
    //    AL->AL_Entry[search_AL_index].deactivated = true; // == ?
    //  else 
    //    AL->AL_Entry[search_AL_index].deactivated = false; // == ?
     if(search_AL_index == AL->size -1) search_AL_index =0 ; else search_AL_index++;
   }
}
/////////////////////////////////////////////////////////////////////
// This function is for handling branch resolution.
//
// Inputs:
// 1. AL_index: Index of the branch in the Active List.
// 2. branch_ID: This uniquely identifies the branch and the
//    checkpoint in question.  It was originally provided
//    by the checkpoint function.
// 3. correct: 'true' indicates the branch was correctly
//    predicted, 'false' indicates it was mispredicted
//    and recovery is required.
//
// Outputs: none.
//
// Tips:
//
// While recovery is not needed in the case of a correct branch,
// some actions are still required with respect to the GBM and
// all checkpointed GBMs:
// * Remember to clear the branch's bit in the GBM.
// * Remember to clear the branch's bit in all checkpointed GBMs.
//
// In the case of a misprediction:
// * Restore the GBM from the checkpoint. Also make sure the
//   mispredicted branch's bit is cleared in the restored GBM,
//   since it is now resolved and its bit and checkpoint are freed.
// * You don't have to worry about explicitly freeing the GBM bits
//   and checkpoints of branches that are after the mispredicted
//   branch in program order. The mere act of restoring the GBM
//   from the checkpoint achieves this feat.
// * Restore other state using the branch's checkpoint.
//   In addition to the obvious state ...  *if* you maintain a
//   freelist length variable (you may or may not), you must
//   recompute the freelist length. It depends on your
//   implementation how to recompute the length.
//   (Note: you cannot checkpoint the length like you did with
//   the head, because the tail can change in the meantime;
//   you must recompute the length in this function.)
// * Do NOT set the branch misprediction bit in the active list.
//   (Doing so would cause a second, full squash when the branch
//   reaches the head of the Active List. We don’t want or need
//   that because we immediately recover within this function.)
/////////////////////////////////////////////////////////////////////
void renamer::resolve(uint64_t AL_index,
	     uint64_t branch_ID,
	     bool correct){

     if(correct==true){
          for(int GBM_pos=0;GBM_pos<n_max_branches;GBM_pos++)
            branch_checkpoint[GBM_pos].GBM = branch_checkpoint[GBM_pos].GBM & ~(1<<branch_ID);
          GBM = GBM & ~(1<<branch_ID);
     }
     else{
          GBM = branch_checkpoint[branch_ID].GBM &  ~(1<<branch_ID);
          FL->head = branch_checkpoint[branch_ID].freelist_head;
          //printf("head of free list is %d,tail is %d\n",FL->head,FL->tail);
          //printf("resolve::Fl empty is %d, head is %d,tail is %d,FL->size is %d\n",FL->empty,FL->head,FL->tail,FL->free_space());
          FL->full=0;
          if(FL->head == FL->tail) FL->empty=1;

          //if(FL->head-FL->tail == 1 || ((FL->tail == FL->size -1)&&(FL->head==0))) FL->empty=1; else FL->empty=0;
          //if(FL->tail-FL->head == 1 || ((FL->head == FL->size -1)&&(FL->tail==0))) FL->full=1;else FL->full=0; 

          for(int i=0;i<n_logical_regs;i++)
             RMT[i]= branch_checkpoint[branch_ID].RMT[i]; 

           RMT_64 = branch_checkpoint[branch_ID].RMT_64;  

          if(AL_index==(AL->size-1)) AL->tail = 0; else AL->tail =AL_index+1;
          if(AL->head == AL->tail && AL->full==1){}
          else 
            AL->full=0;AL->empty=0;
          //printf("in resolve function AL->tail is %d AL_index is %d\n",AL->tail,AL_index);

          //if(AL->head-AL->tail == 1 || ((AL->tail == AL->size -1)&&(AL->head==0))) AL->full=1; else AL->full=0;
          //if(AL->tail-AL->head == 1 || ((AL->head == AL->size -1)&&(AL->tail==0))) AL->empty=1;else AL->empty=0; 
 
     }  
}

//////////////////////////////////////////
// Functions related to Retire Stage.   //
//////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// This function allows the caller to examine the instruction at the head
// of the Active List.
//
// Input arguments: none.
//
// Return value:
// * Return "true" if the Active List is NOT empty, i.e., there
//   is an instruction at the head of the Active List.
// * Return "false" if the Active List is empty, i.e., there is
//   no instruction at the head of the Active List.
//
// Output arguments:
// Simply return the following contents of the head entry of
// the Active List.  These are don't-cares if the Active List
// is empty (you may either return the contents of the head
// entry anyway, or not set these at all).
// * completed bit
// * exception bit
// * load violation bit
// * branch misprediction bit
// * value misprediction bit
// * load flag (indicates whether or not the instr. is a load)
// * store flag (indicates whether or not the instr. is a store)
// * branch flag (indicates whether or not the instr. is a branch)
// * amo flag (whether or not instr. is an atomic memory operation)
// * csr flag (whether or not instr. is a system instruction)
// * program counter of the instruction
/////////////////////////////////////////////////////////////////////
bool renamer::precommit(bool &completed,
               bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
               bool &load, bool &store, bool &branch, bool &amo, bool &csr,
	       uint64_t &PC, bool &deactivated){
     completed = AL->AL_Entry[AL->head].completed;
     exception = AL->AL_Entry[AL->head].exception;
     load_viol = AL->AL_Entry[AL->head].load_violation;
     br_misp   = AL->AL_Entry[AL->head].branch_mispred;
     val_misp  = AL->AL_Entry[AL->head].value_mispred;
     load      = AL->AL_Entry[AL->head].load_flag;
     store     = AL->AL_Entry[AL->head].store_flag;
     branch    = AL->AL_Entry[AL->head].branch_flag;
     amo       = AL->AL_Entry[AL->head].amo_flag;
     csr       = AL->AL_Entry[AL->head].csr_flag;
     PC        = AL->AL_Entry[AL->head].PC;
     deactivated        = AL->AL_Entry[AL->head].deactivated;



     if(deactivated){ exception =false;load_viol=false;}
     
     //printf("pre commit is called\n");
     if(!AL->empty) {
          //printf("AL not empty\n");
          return true;}
     else {
          //printf("AL is empty\n");
          return false;}

}

/////////////////////////////////////////////////////////////////////
// This function commits the instruction at the head of the Active List.
//
// Tip (optional but helps catch bugs):
// Before committing the head instruction, assert that it is valid to
// do so (use assert() from standard library). Specifically, assert
// that all of the following are true:
// - there is a head instruction (the active list isn't empty)
// - the head instruction is completed
// - the head instruction is not marked as an exception
// - the head instruction is not marked as a load violation
// It is the caller's (pipeline's) duty to ensure that it is valid
// to commit the head instruction BEFORE calling this function
// (by examining the flags returned by "precommit()" above).
// This is why you should assert() that it is valid to commit the
// head instruction and otherwise cause the simulator to exit.
/////////////////////////////////////////////////////////////////////
void renamer::commit(){
   uint64_t dest_logic_reg;
   uint64_t old_phy_reg;

   //printf("all assert conditions are starting\n");
//assert((!AL->empty) && (AL->AL_Entry[AL->head].completed==true) && (AL->AL_Entry[AL->head].exception==false) && (AL->AL_Entry[AL->head].load_violation==false));
assert(AL->empty==false);
assert(AL->AL_Entry[AL->head].completed==true) ;
assert(AL->AL_Entry[AL->head].exception==false);
assert(AL->AL_Entry[AL->head].load_violation==false);

   //printf("all assert conditions are true\n");
   if(AL->AL_Entry[AL->head].dest_valid==1){
      dest_logic_reg=AL->AL_Entry[AL->head].dest_logic_reg;
      if(AL->AL_Entry[AL->head].deactivated ==false){
        if(dest_logic_reg == 64){
          old_phy_reg = AMT_64;
          AMT_64 = AL->AL_Entry[AL->head].dest_phy_reg;
          FL->FL_Entry[FL->tail] = old_phy_reg; //commiting the new dest phy reg
        }
        else{
          old_phy_reg = AMT[dest_logic_reg]; //retrieving old phy dest reg for logical reg
          AMT[dest_logic_reg] = AL->AL_Entry[AL->head].dest_phy_reg; //commiting the new dest phy reg
          FL->FL_Entry[FL->tail] = old_phy_reg; //adding the old dest phy reg into free_list
        }
        //printf("new fl_entry at tail %d is :%d\n",FL->tail,FL->FL_Entry[FL->tail]);
      }
      else FL->FL_Entry[FL->tail] = AL->AL_Entry[AL->head].dest_phy_reg; 

      if(FL->full==1) FL->full=0;
      if(FL->head-FL->tail == 1 || ((FL->tail == FL->size -1)&&(FL->head==0))) FL->empty=1; 
      if(FL->tail == FL->size-1) FL->tail=0; else FL->tail++; 
      //printf("commit::Fl empty is %d, head is %d,tail is %d,FL->size is %d\n",FL->empty,FL->head,FL->tail,FL->free_space());
   } 
   else {//printf("no valid destination at head :%d\n",AL->head);
   }
   if(AL->full==1) AL->full=0; 
   if(AL->tail-AL->head == 1 || ((AL->head == AL->size -1)&&(AL->tail==0))) AL->empty=1; 
   if(AL->head == AL->size-1) AL->head=0; else AL->head++; 
   //printf("commit::AL full is %d, head is %d,tail is %d,AL->left size is %d\n",AL->full,AL->head,AL->tail,AL->free_space());

}

//////////////////////////////////////////////////////////////////////
// Squash the renamer class.
//
// Squash all instructions in the Active List and think about which
// sructures in your renamer class need to be restored, and how.
//
// After this function is called, the renamer should be rolled-back
// to the committed state of the machine and all renamer state
// should be consistent with an empty pipeline.
/////////////////////////////////////////////////////////////////////
void renamer::squash(){

    for(int i=0;i<n_logical_regs;i++){
     RMT[i].phy_reg=AMT[i];
     RMT[i].t_valid=false;
     RMT[i].e_valid=false;
    }
    AL->tail= AL->head;
    FL->head= FL->tail; 
    
    AL->empty=1;
    FL->empty=1;
    AL->full=0;
    FL->full=0;
    GBM =0; 
    //printf("squash function called\n");

}

//////////////////////////////////////////
// Functions not tied to specific stage.//
//////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// Functions for individually setting the exception bit,
// load violation bit, branch misprediction bit, and
// value misprediction bit, of the indicated entry in the Active List.
/////////////////////////////////////////////////////////////////////
void renamer::set_exception(uint64_t AL_index){
   AL->AL_Entry[AL_index].exception=true;
}
void renamer::set_load_violation(uint64_t AL_index){
   AL->AL_Entry[AL_index].load_violation=true;
}
void renamer::set_branch_misprediction(uint64_t AL_index){
   AL->AL_Entry[AL_index].branch_mispred=true;

}
void renamer::set_value_misprediction(uint64_t AL_index){
   AL->AL_Entry[AL_index].value_mispred=true;
}

/////////////////////////////////////////////////////////////////////
// Query the exception bit of the indicated entry in the Active List.
/////////////////////////////////////////////////////////////////////
bool renamer::get_exception(uint64_t AL_index){
   return AL->AL_Entry[AL_index].exception;
}

