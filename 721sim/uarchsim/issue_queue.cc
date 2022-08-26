#include "pipeline.h"


// constructor
issue_queue::issue_queue(unsigned int size, unsigned int num_parts, pipeline_t* _proc):proc(_proc) {
	// Initialize the issue queue.
	q = new issue_queue_entry_t[size];
	this->size = size;
	length = 0;
	for (unsigned int i = 0; i < size; i++) {
		q[i].valid = false;
	}

	assert((size % num_parts) == 0);
        this->part_size = (size/num_parts);
        this->part_next = 0;

	// Initialize the issue queue's free list.
	fl = new unsigned int[size];
	fl_head = 0;
	fl_tail = 0;
	fl_length = size;
	for (unsigned int i = 0; i < size; i++) {
		fl[i] = i;
	}

	// Issue queue is initially empty, so oldest and youngest indices are initially invalid.
	oldest = -1;
	youngest = -1;

  // Needed for macro
  stats = proc->get_stats();
}

bool issue_queue::stall(unsigned int bundle_inst) {
	assert((length + fl_length) == size);
  ifprintf(logging_on,proc->dispatch_log,"ISSUE_QUEUE: Checking for stall fl_length: %u  length: %u bundle_inst: %u stall: %s\n",fl_length,length,bundle_inst,(fl_length < bundle_inst)?"true":"false");
	return(fl_length < bundle_inst);
}

void issue_queue::dispatch(unsigned int index, unsigned long long branch_mask, unsigned int lane_id,
                           bool A_valid, bool A_ready, unsigned int A_tag,
                           bool B_valid, bool B_ready, unsigned int B_tag,
                           bool D_valid, bool D_ready, unsigned int D_tag) {
	unsigned int free;

	// Assert there is a free issue queue entry.
	assert(fl_length > 0);
	assert(length < size);

	// Pop a free issue queue entry.
	free = fl[fl_head];
	fl_head = MOD_S((fl_head + 1), size);
	fl_length--;

	// Dispatch the instruction into the free issue queue entry.
	length++;
	assert(!q[free].valid);
	q[free].valid = true;
	q[free].index = index;
	q[free].branch_mask = branch_mask;
	q[free].lane_id = lane_id;
	q[free].A_valid = A_valid;
	q[free].A_ready = A_ready;
	q[free].A_tag = A_tag;
	q[free].B_valid = B_valid;
	q[free].B_ready = B_ready;
	q[free].B_tag = B_tag;
	q[free].D_valid = D_valid;
	q[free].D_ready = D_ready;
	q[free].D_tag = D_tag;

	// Add this instruction to tail of linked-list for ideal age-based priority.
	if (oldest == -1) {	// IQ empty
	   assert(youngest == -1);
	   q[free].prev = -1;
	   q[free].next = -1;
	   oldest = free;
	   youngest = free;
	}
	else {			// IQ not empty
	   assert(youngest != -1);
	   assert(q[youngest].next == -1);
	   q[free].prev = youngest;
	   q[free].next = -1;
	   q[youngest].next = free;
	   youngest = free;
	}
}

void issue_queue::wakeup(unsigned int tag) {
	// Broadcast the tag to every entry in the issue queue.
	// If the broadcasted tag matches a valid tag:
	// (1) Assert that the ready bit is initially false because if someone is 
  //      broadcasting a tag, that source can not already be valid
	// (2) Set the ready bit.
  

  inc_counter(wakeup_cam_read_count);

	for (unsigned int i = 0; i < size; i++) {
		if (q[i].valid) {					// Only consider valid issue queue entries.
			if (q[i].A_valid && (tag == q[i].A_tag)) {	// Check first source operand.
				assert(!q[i].A_ready);
				q[i].A_ready = true;
        #ifdef RISCV_MICRO_DEBUG
          LOG(proc->issue_log,proc->cycle,proc->PAY.buf[q[i].index].sequence,proc->PAY.buf[q[i].index].pc,"Waking up RS1 iq entry %u",i);
          dump_iq(proc,i,proc->issue_log);
        #endif

			}
			if (q[i].B_valid && (tag == q[i].B_tag)) {	// Check second source operand.
				assert(!q[i].B_ready);
				q[i].B_ready = true;
        #ifdef RISCV_MICRO_DEBUG
          LOG(proc->issue_log,proc->cycle,proc->PAY.buf[q[i].index].sequence,proc->PAY.buf[q[i].index].pc,"Waking up RS2 iq entry %u",i);
          dump_iq(proc,i,proc->issue_log);
        #endif
			}
			if (q[i].D_valid && (tag == q[i].D_tag)) {	// Check third source operand.
				assert(!q[i].D_ready);
				q[i].D_ready = true;
        #ifdef RISCV_MICRO_DEBUG
          LOG(proc->issue_log,proc->cycle,proc->PAY.buf[q[i].index].sequence,proc->PAY.buf[q[i].index].pc,"Waking up RS3 iq entry %u",i);
          dump_iq(proc,i,proc->issue_log);
        #endif
			}
		}
	}
}

void issue_queue::select_and_issue(unsigned int num_lanes, lane* Execution_Lanes) {
   unsigned int i, j;
   bool issue;
   unsigned int dyn_lane_id;
   bool issuedThisCycle = false;

   // Set up the first IQ index to be examined this cycle.
   if (IDEAL_AGE_BASED) {
      if (oldest == -1) { // IQ empty, so no age-based list to sequence through.
         assert(youngest == -1);
	 assert(length == 0);
         return;
      }
      else {
         i = (unsigned int)oldest;  // Start sequencing at oldest instruction in the IQ.
      }
   }
   else {
      i = part_next;
   }

   // The same 'j' loop supports both of the following modes:
   // - scan the entire IQ sequentially from the first index i
   // - scan valid IQ entries in age-order from the first (oldest) index i
   for (j = 0; j < size; j++) {
      assert(!IDEAL_AGE_BASED || q[i].valid);
 
      // Check if the instruction is valid and ready.
      if (q[i].valid && (!q[i].A_valid || q[i].A_ready) && (!q[i].B_valid || q[i].B_ready) && (!q[i].D_valid || q[i].D_ready)) {
         if (PRESTEER) {
            // Check if the instruction's desired Execution Lane is free.
	    issue = !Execution_Lanes[q[i].lane_id].rr.valid;
	 }
	 else {
	    // Check if there is a free Execution Lane among all candidate lanes.
            issue = false;
	    dyn_lane_id = 0;
	    while (!issue && (dyn_lane_id < num_lanes)) {
	       if ((q[i].lane_id & (1 << dyn_lane_id)) && !Execution_Lanes[dyn_lane_id].rr.valid) {
		  issue = true;
                  q[i].lane_id = dyn_lane_id;
	       }
	       else {
	          dyn_lane_id++;
	       }
	    }
         }

	 if (issue) {
            assert(q[i].lane_id < num_lanes);
            assert(!Execution_Lanes[q[i].lane_id].rr.valid);

            // Issue the instruction to the Register Read Stage within the Execution Lane.
            Execution_Lanes[q[i].lane_id].rr.valid = true;
            Execution_Lanes[q[i].lane_id].rr.index = q[i].index;
            Execution_Lanes[q[i].lane_id].rr.branch_mask = q[i].branch_mask;

            // Remove the instruction from the issue queue.
            remove(i);
   
            issuedThisCycle = true;
            inc_counter(issued_inst_count);
         }
      }

      if (IDEAL_AGE_BASED) {
	 // Set q index to that of the next-oldest instruction, or break from loop if there is no next-oldest instruction.
	 // Note: even if we issued and removed i from the IQ, above, its next pointer is still available.
         if (q[i].next == -1)
	    break;
         else
	    i = (unsigned int)q[i].next;
      }
      else {
         // Modulo increment q index.
         i++;
         if (i == size)
            i = 0;
      }
   }

   if (issuedThisCycle)
      inc_counter(issued_bundle_count);

   // Set up the next partition based on round-robin.
   part_next += part_size;
   if (part_next == size)
      part_next = 0;
}

void issue_queue::remove(unsigned int i) {
	assert(length > 0);
	assert(fl_length < size);

	// Remove the instruction from the issue queue.
	q[i].valid = false;
	length--;

	// Push the issue queue entry back onto the free list.
	fl[fl_tail] = i;
	fl_tail = MOD_S((fl_tail + 1), size);
	fl_length++;

	// Update the linked-list used for ideal age-based ordering.
	assert((oldest != -1) && (youngest != -1));
	if ((oldest == i) && (youngest == i)) {	// i is the only instr. in list
	   assert(length == 0);			// decremented from 1 to 0 above
	   assert(q[i].prev == -1);
	   assert(q[i].next == -1);
	   oldest = -1;
	   youngest = -1;
	}
	else if (oldest == i) { // i is oldest instr. in list, and not youngest
	   assert(q[i].prev == -1);
	   assert(q[i].next != -1);
	   assert(q[q[i].next].prev == i);
	   q[q[i].next].prev = -1;
	   oldest = q[i].next;
	}
	else if (youngest == i) { // i is youngest instr. in list, and not oldest
	   assert(q[i].next == -1);
	   assert(q[i].prev != -1);
	   assert(q[q[i].prev].next == i);
	   q[q[i].prev].next = -1;
	   youngest = q[i].prev;
	}
	else { // i is neither the oldest nor the youngest instr. in list
	   assert(q[i].prev != -1);
	   assert(q[i].next != -1);
	   assert(q[q[i].prev].next == i);
	   assert(q[q[i].next].prev == i);
	   q[q[i].prev].next = q[i].next;
	   q[q[i].next].prev = q[i].prev;
	}
}

void issue_queue::flush() {
	length = 0;
	for (unsigned int i = 0; i < size; i++) {
		q[i].valid = false;
	}

	fl_head = 0;
	fl_tail = 0;
	fl_length = size;
	for (unsigned int i = 0; i < size; i++) {
		fl[i] = i;
	}

	oldest = -1;
	youngest = -1;
}

void issue_queue::clear_branch_bit(unsigned int branch_ID) {
	for (unsigned int i = 0; i < size; i++) {
		CLEAR_BIT(q[i].branch_mask, branch_ID);
	}
}

void issue_queue::squash(unsigned int branch_ID) {
	for (unsigned int i = 0; i < size; i++) {
		if (q[i].valid && BIT_IS_ONE(q[i].branch_mask, branch_ID)) {
			remove(i);
		}
	}
}


void issue_queue::dump_iq(pipeline_t* proc, unsigned int index,FILE* file)
{
  proc->disasm(proc->PAY.buf[q[index].index].inst,proc->cycle,proc->PAY.buf[q[index].index].pc,proc->PAY.buf[q[index].index].sequence,file);
  ifprintf(logging_on,file,"fl_head %d fl_tail %d fl_length %d\n",fl_head, fl_tail, fl_length);
  ifprintf(logging_on,file,"valid      : %u\t",           q[index].valid);
  ifprintf(logging_on,file,"branch_mask: %" PRIu64 "\t",  q[index].branch_mask);
  ifprintf(logging_on,file,"lane_id    : %u\t",           q[index].lane_id);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"RS1_Valid  : %u\t",           q[index].A_valid);
  ifprintf(logging_on,file,"RS1_Ready  : %u\t",           q[index].A_ready);
  ifprintf(logging_on,file,"RS1_tag    : %u\t",           q[index].A_tag);
  ifprintf(logging_on,file,"RS2_Valid  : %u\t",           q[index].B_valid);
  ifprintf(logging_on,file,"RS2_Ready  : %u\t",           q[index].B_ready);
  ifprintf(logging_on,file,"RS2_tag    : %u\t",           q[index].B_tag);
  if(unlikely(q[index].D_valid)){
    ifprintf(logging_on,file,"RS3_Valid  : %u\t",           q[index].D_valid);
    ifprintf(logging_on,file,"RS3_Ready  : %u\t",           q[index].D_ready);
    ifprintf(logging_on,file,"RS3_tag    : %u\t",           q[index].D_tag);
  }
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"\n");
}

