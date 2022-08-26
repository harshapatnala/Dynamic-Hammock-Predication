#include <stdlib.h>
#include <cinttypes>

#include "parameters.h"
#include "histogram.h"
#include "CacheClass.h"
#include "lsu.h"
#include "mmu.h"
#include "pipeline.h"
#include "trap.h"
#include "stats.h"


bool lsu::disambiguate(unsigned int lq_index,
                       unsigned int sq_index, bool sq_index_phase,
                       bool& forward,
                       unsigned int& store_entry) {
	bool stall;		// return value
	uint64_t max_size;
	uint64_t mask;

	// Check if the load is logically at the head of the SQ, i.e., no prior stores.
	if ((sq_index == sq_head) && (sq_index_phase == sq_head_phase)) {
		// There are no stores prior to the load.
		stall = false;
		forward = false;
	}
	else {
		stall = false;
		forward = false;

		// Because the load is not logically at the head of the SQ,
		// it must be true that the SQ has at least one store.
		assert(sq_length > 0);

		store_entry = sq_index;
		do {
			store_entry = MOD_S((store_entry + sq_size - 1), sq_size);

			max_size = MAX(SQ[store_entry].size, LQ[lq_index].size);
			mask = (~(max_size - 1));

			if (!SQ[store_entry].addr_avail) {
				stall = LQ[lq_index].mdp_stall;  // stall (if prediction says to): possible conflict
			}
			else if ((SQ[store_entry].addr & mask) ==
			         (LQ[lq_index].addr    & mask)) {
				// There is a conflict.
				if (SQ[store_entry].size != LQ[lq_index].size) {
					stall = true;    // stall: partial conflict scenarios are hard
				}
				else if (!SQ[store_entry].value_avail) {
					stall = true;    // stall: must wait for value to be available
				}
				else {
					forward = true;    // forward: sizes match and value is available
				}
			}
		} while ((store_entry != sq_head) && !stall && !forward);
	}

	return(stall);
}	// disambiguate()

bool lsu::ld_violation(unsigned int sq_index,
                       unsigned int lq_index, bool lq_index_phase,
                       unsigned int& load_entry) {
   bool misp;
   bool load_entry_phase;
   uint64_t max_size;
   uint64_t mask;

   misp = false;
   load_entry = lq_index;
   load_entry_phase = lq_index_phase;

   // Search the LQ from the first load after the store (if it exists) to the tail.
   while (!misp && !((load_entry == lq_tail) && (load_entry_phase == lq_tail_phase))) {
      max_size = MAX(SQ[sq_index].size, LQ[load_entry].size);
      mask = (~(max_size - 1));

      if (LQ[load_entry].value_avail && ((SQ[sq_index].addr & mask) == (LQ[load_entry].addr & mask))) {
         misp = true;
      }
      else {
         load_entry = MOD_S((load_entry + 1), lq_size);
         if (load_entry == 0)   // wrapped around, so toggle phase bit
            load_entry_phase = !load_entry_phase;
      }
   }

   return(misp);
} // ld_violation()

void lsu::set_l2_cache(CacheClass* l2_dc){
	DC->set_nextLevel(l2_dc);
}

lsu::lsu(unsigned int lq_size, unsigned int sq_size, unsigned int Tid, mmu_t* _mmu, pipeline_t* _proc):
      proc(_proc),
      mmu(_mmu)
{

	this->Tid = Tid;

	DC = new CacheClass(  L1_DC_SETS,
          	            L1_DC_ASSOC,
          	            L1_DC_LINE_SIZE,
          	            L1_DC_HIT_LATENCY,
          	            L1_DC_MISS_LATENCY,
          	            L1_DC_NUM_MHSRs,
          	            L1_DC_MISS_SRV_PORTS,
          	            L1_DC_MISS_SRV_LATENCY,
                        _proc,
                        "l1_dc",
                        _proc->L2C);

	// LQ initialization.
	this->lq_size = lq_size;
	lq_head = 0;
	lq_head_phase = false;
	lq_tail = 0;
	lq_tail_phase = false;
	lq_length = 0;

	LQ = new lsq_entry[lq_size];
	for (unsigned int i = 0; i < lq_size; i++) {
		LQ[i].valid = false;
	}

	// SQ initialization.
	this->sq_size = sq_size;
	sq_head = 0;
	sq_head_phase = false;
	sq_tail = 0;
	sq_tail_phase = false;
	sq_length = 0;

	SQ = new lsq_entry[sq_size];
	for (unsigned int i = 0; i < sq_size; i++) {
		SQ[i].valid = false;
  }

	// STATS
	n_stall_disambig = 0;
	n_forward = 0;
	n_stall_miss_l = 0;
	n_stall_miss_s = 0;
	n_load = 0;
	n_store = 0;
}

lsu::~lsu(){
  delete DC;
}

bool lsu::stall(unsigned int bundle_load, unsigned int bundle_store) {
  bool store_stall = (sq_length + bundle_store) > sq_size;
  bool load_stall = (lq_length + bundle_load)  > lq_size;
  ifprintf(logging_on,proc->dispatch_log,"LSQ: Checking for stall lq_length: %u  sq_length: %u bundle_load: %u bundle_store: %u load_stall: %s store_stall %s\n",
                lq_length,sq_length,bundle_load,bundle_store,load_stall?"true":"false",store_stall?"true":"false");
	return( load_stall || store_stall );
}


void lsu::dispatch(bool load,
                   unsigned int size,
                   bool left,
                   bool right,
                   bool is_signed,
		   bool amo,
                   unsigned int pay_index,
                   unsigned int& lq_index, bool& lq_index_phase,
                   unsigned int& sq_index, bool& sq_index_phase,
                   bool mdp_stall) {
	// Assign indices to the load or store.
	lq_index = lq_tail;
	lq_index_phase = lq_tail_phase;
	sq_index = sq_tail;
	sq_index_phase = sq_tail_phase;

	if (load) {
		// Assert that the LQ isn't full.
		assert(lq_length < lq_size);

		// Allocate entry in the LQ.
		LQ[lq_tail].valid = true;
		LQ[lq_tail].is_signed = is_signed;
		//LQ[lq_tail].left = left;
		//LQ[lq_tail].right = right;
		LQ[lq_tail].size = size;
		LQ[lq_tail].amo = amo;
		LQ[lq_tail].addr_avail = false;
		LQ[lq_tail].value_avail = false;
		LQ[lq_tail].missed = false;

		LQ[lq_tail].pay_index = pay_index;
		LQ[lq_tail].sq_index = sq_index;
		LQ[lq_tail].sq_index_phase = sq_index_phase;

                LQ[lq_tail].mdp_stall = mdp_stall;

		// STATS
		LQ[lq_tail].stat_load_stall_disambig = false;
		LQ[lq_tail].stat_load_stall_miss = false;
		LQ[lq_tail].stat_store_stall_miss = false;
		LQ[lq_tail].stat_forward = false;

    #ifdef RISCV_MICRO_DEBUG
      LOG(proc->lsu_log,proc->cycle,proc->PAY.buf[pay_index].sequence,proc->PAY.buf[pay_index].pc,"Dispatching load lq entry %u",lq_tail);
      dump_lq(proc,lq_tail,proc->lsu_log);
    #endif

		// Advance LQ tail pointer and increment LQ length.
		lq_tail = MOD_S((lq_tail + 1), lq_size);
		lq_length++;

		// Detect wrap-around of lq_tail and toggle its phase bit accordingly.
		if (lq_tail == 0) {
			lq_tail_phase = !lq_tail_phase;
		}
	}
	else {
		// Assert that the SQ isn't full.
		assert(sq_length < sq_size);

		// Allocate entry in the SQ.
		SQ[sq_tail].valid = true;
		SQ[sq_tail].is_signed = is_signed;
		//SQ[sq_tail].left = left;
		//SQ[sq_tail].right = right;
		SQ[sq_tail].size = size;
		SQ[sq_tail].amo = amo;
		SQ[sq_tail].addr_avail = false;
		SQ[sq_tail].value_avail = false;
		SQ[sq_tail].missed = false;

		SQ[sq_tail].pay_index = pay_index;

		// STATS
		SQ[sq_tail].stat_load_stall_disambig = false;
		SQ[sq_tail].stat_load_stall_miss = false;
		SQ[sq_tail].stat_store_stall_miss = false;
		SQ[sq_tail].stat_forward = false;

    #ifdef RISCV_MICRO_DEBUG
      LOG(proc->lsu_log,proc->cycle,proc->PAY.buf[pay_index].sequence,proc->PAY.buf[pay_index].pc,"Dispatching store sq entry %u",sq_tail);
      dump_sq(proc,sq_tail,proc->lsu_log);
    #endif


		// Advance SQ tail pointer and increment SQ length.
		sq_tail = MOD_S((sq_tail + 1), sq_size);
		sq_length++;

		// Detect wrap-around of sq_tail and toggle its phase bit accordingly.
		if (sq_tail == 0) {
			sq_tail_phase = !sq_tail_phase;
		}

	}
}


void lsu::store_addr(cycle_t cycle,
                     reg_t addr,
                     unsigned int sq_index,
                     unsigned int lq_index, bool lq_index_phase) {
   assert(SQ[sq_index].valid);

   inc_counter(spec_store_count);

   // Oracle memory disambiguation results in the store address being recorded twice:
   // the oracle store address at dispatch time and the computed store address at
   // execute time. If the address was already recorded at dispatch time, confirm that
   // it matches the one recorded at execute time (now). Then exit the function since
   // we do not want to access the Data Cache for a second time.
   if (SQ[sq_index].addr_avail) {
      // This assert will fail for a store address generation on the wrong path.
      // It will calculate the wrong value through wrong data dependence chain.
      // This is functionally correct and hence removing this assert. This assert 
      // will fail even with perfect branch prediction due to mispredictions and 
      // exceptions caused by CSR instructions.
      // WHY? the assert
      //assert(SQ[sq_index].addr == addr);
      return;
   }

   SQ[sq_index].addr_avail = true;
   SQ[sq_index].addr = addr;

   // Detect and mark load violations.
   if (SPEC_DISAMBIG) {
      unsigned int load_entry;
      unsigned int al_index;
      if (ld_violation(sq_index, lq_index, lq_index_phase, load_entry)) {
         al_index = proc->PAY.buf[LQ[load_entry].pay_index].AL_index;
         proc->set_load_violation(al_index);
      }
   }

   if (!PERFECT_DCACHE) {
      bool hit;
      SQ[sq_index].miss_resolve_cycle = DC->Access(Tid, cycle, addr, true, &hit);
      SQ[sq_index].missed = !hit;

      if (!hit) inc_counter(spec_store_miss_count);
      if (SQ[sq_index].miss_resolve_cycle == -1) inc_counter(store_mhsr_miss_count);
   }

#ifdef RISCV_MICRO_DEBUG
   LOG(proc->lsu_log,proc->cycle,proc->PAY.buf[SQ[sq_index].pay_index].sequence,proc->PAY.buf[SQ[sq_index].pay_index].pc,"Executing store sq entry %u",sq_index);
   dump_sq(proc,sq_index,proc->lsu_log);
#endif
}


void lsu::store_value(unsigned int sq_index, reg_t value) {
	assert(SQ[sq_index].valid);

	SQ[sq_index].value_avail = true;
	SQ[sq_index].value = value;
}


bool lsu::load_addr(cycle_t cycle,
                    reg_t addr,
                    unsigned int lq_index, bool lq_index_phase,
                    unsigned int sq_index, bool sq_index_phase,
                    //reg_t back_data, // LWL/LWR
                    reg_t& value) {
	assert(LQ[lq_index].valid);

	// Set up information for executing the load.
	LQ[lq_index].addr_avail = true;
	LQ[lq_index].addr = addr;
	//LQ[lq_index].back_data = back_data;

  #ifdef RISCV_MICRO_DEBUG
    LOG(proc->lsu_log,proc->cycle,proc->PAY.buf[LQ[lq_index].pay_index].sequence,proc->PAY.buf[LQ[lq_index].pay_index].pc,"Executing load lq entry %u",lq_index);
    dump_lq(proc,lq_index,proc->lsu_log);
  #endif

	if (!PERFECT_DCACHE) {
		bool hit;
		LQ[lq_index].miss_resolve_cycle = DC->Access(Tid, cycle, addr, false, &hit);
		LQ[lq_index].missed = !hit;
    if(!hit){
      inc_counter(spec_load_miss_count);
    }
    if(LQ[lq_index].miss_resolve_cycle == -1){
      inc_counter(load_mhsr_miss_count);
    }

	}

	// Run the load through the load execution datapath.
	execute_load(cycle, lq_index, lq_index_phase, sq_index, sq_index_phase);

	// Result of running the load through the load execution datapath.
	value = LQ[lq_index].value;
	return(LQ[lq_index].value_avail);
}

bool lsu::load_unstall(cycle_t cycle, unsigned int& pay_index, reg_t& value) {
   unsigned int scan = lq_head;
   bool scan_phase = lq_head_phase;
   bool unstalled = false;
   while (!((scan == lq_tail) && (scan_phase == lq_tail_phase)) && !unstalled) {
      assert(LQ[scan].valid);
      if (LQ[scan].addr_avail && !LQ[scan].value_avail) {
         // If this load did not get an MHSR during initial execution, access the D$ again.
         if (!PERFECT_DCACHE && (LQ[scan].miss_resolve_cycle == -1)) {
            bool hit;
            assert(LQ[scan].addr_avail);
            LQ[scan].miss_resolve_cycle = DC->Access(Tid, cycle, LQ[scan].addr, false, &hit);
            LQ[scan].missed = !hit;
         }

         // Check if load is unstalled.
         execute_load(cycle, scan, scan_phase, LQ[scan].sq_index, LQ[scan].sq_index_phase);
         unstalled = LQ[scan].value_avail;
         pay_index = LQ[scan].pay_index;
         value = LQ[scan].value;
      }
      scan = MOD_S((scan + 1), lq_size);
      if (scan == 0) // wrap-around, i.e., phase change
         scan_phase = !scan_phase;
   }
   return(unstalled);
}

void lsu::execute_load(cycle_t cycle,
                       unsigned int lq_index, bool lq_index_phase,
                       unsigned int sq_index, bool sq_index_phase) {
	bool stall_disambig;
	bool forward;
	unsigned int store_entry;

	assert(LQ[lq_index].valid);
	assert(LQ[lq_index].addr_avail);
	assert(!LQ[lq_index].value_avail);

        if (LQ[lq_index].amo) {   // Load reservation.
           if ((lq_index == lq_head) && (lq_index_phase == lq_head_phase)) {
	      // Load reservation has reached the head of the LQ and may proceed with the reservation.
              // Set up load reservation to verifiy atomicity with a following store-conditional.
              proc->get_state()->load_reservation = LQ[lq_index].addr;
           }
           else {
	      // Load reservation has not yet reached the head of the LQ and must stall.
	      return;
	   }
        }

  inc_counter(spec_load_count);

	stall_disambig = disambiguate(lq_index, sq_index, sq_index_phase, forward, store_entry);

  #ifdef RISCV_MICRO_DEBUG
    LOG(proc->lsu_log,proc->cycle,proc->PAY.buf[LQ[lq_index].pay_index].sequence,proc->PAY.buf[LQ[lq_index].pay_index].pc,"Executing load lq entry %u",lq_index);
    dump_lq(proc,lq_index,proc->lsu_log);
  #endif

	if (stall_disambig) {
		// STATS
		LQ[lq_index].stat_load_stall_disambig = true;
	}
	else if (forward) {
		// STATS
		LQ[lq_index].stat_forward = true;

		// Forward the data from the store entry.
		switch (LQ[lq_index].size) {
			case 1:
				if (LQ[lq_index].is_signed) {
					LQ[lq_index].value = (sreg_t)((int8_t)(SQ[store_entry].value));
				}
				else {
					LQ[lq_index].value = (reg_t)((uint8_t)(SQ[store_entry].value));
				}
				break;

			case 2:
				if (LQ[lq_index].is_signed) {
					LQ[lq_index].value = (sreg_t)((int16_t)(SQ[store_entry].value));
				}
				else {
					LQ[lq_index].value = (reg_t)((uint16_t)(SQ[store_entry].value));
				}
				break;

			case 4:
				if (LQ[lq_index].is_signed) {
					LQ[lq_index].value = (sreg_t)((int32_t)(SQ[store_entry].value));
				}
				else {
					LQ[lq_index].value = (reg_t)((uint32_t)(SQ[store_entry].value));
				}
				break;

			case 8:
				LQ[lq_index].value = SQ[store_entry].value;
				break;

			default:
				assert(0);
				break;
		}

		// The load value is now available.
		LQ[lq_index].value_avail = true;
	}
  // If either a hit or a miss and the line has already been loaded
	else if (!(LQ[lq_index].missed && (cycle < LQ[lq_index].miss_resolve_cycle))) {
		// Load data from memory.
    // MMU takes care of checking whether memory within bounds, throws exception otherwise.

    // Catch MMU and MEMORY exceptions here.
    // 1) Mark the exception bit in Active List so that the exception is delayed to retirement.
    // There are two main reasons for this. First, this halts retirement of further instructions.
    // But more importantly, this is how precise exception can be implemented in an OOO machine
    // and keep the flow of instructions correct.
    // 2) Mark the appropriate cause bit in the exception vector so that the correct exception 
    // handler can be invoked at retire.
    try{
			switch (LQ[lq_index].size) {
				case 1:
					if (LQ[lq_index].is_signed) {
						LQ[lq_index].value = (sreg_t)mmu->load_int8(LQ[lq_index].addr);
					}
					else {
						LQ[lq_index].value = (reg_t)mmu->load_uint8(LQ[lq_index].addr);
					}
					break;

				case 2:
					if (LQ[lq_index].is_signed) {
						LQ[lq_index].value = (sreg_t)mmu->load_int16(LQ[lq_index].addr);
					}
					else {
						LQ[lq_index].value = (reg_t)mmu->load_uint16(LQ[lq_index].addr);
					}
					break;

				case 4:
					if (LQ[lq_index].is_signed) {
						LQ[lq_index].value = (sreg_t)mmu->load_int32(LQ[lq_index].addr);
					}
					else {
						LQ[lq_index].value = (reg_t)mmu->load_uint32(LQ[lq_index].addr);
					}
					break;

				case 8:
					LQ[lq_index].value = (reg_t)mmu->load_uint64(LQ[lq_index].addr);
					break;

				default:
					assert(0);
					break;
			}
    } 
    catch (mem_trap_t& t)
	  {
      unsigned int al_index = proc->PAY.buf[LQ[lq_index].pay_index].AL_index;
      reg_t epc = proc->PAY.buf[LQ[lq_index].pay_index].pc;
		  ifprintf(logging_on,proc->lsu_log, "Cycle %" PRIcycle ": core %3d: load exception %s, epc 0x%016" PRIx64 " badvaddr 0x%16" PRIx64 "\n",
		          proc->cycle, proc->id, t.name(), epc, t.get_badvaddr());

      assert(t.cause() == CAUSE_FAULT_LOAD || t.cause() == CAUSE_MISALIGNED_LOAD);
      proc->set_exception(al_index);
      proc->PAY.buf[LQ[lq_index].pay_index].trap.post(t);
	  }

		// The load value is now available.
		LQ[lq_index].value_avail = true;
	}
	else {
		// STATS
		LQ[lq_index].stat_load_stall_miss = true;
	}

}


void lsu::checkpoint(unsigned int& chkpt_lq_tail, bool& chkpt_lq_tail_phase,
                     unsigned int& chkpt_sq_tail, bool& chkpt_sq_tail_phase) {
	chkpt_lq_tail = lq_tail;
	chkpt_lq_tail_phase = lq_tail_phase;
	chkpt_sq_tail = sq_tail;
	chkpt_sq_tail_phase = sq_tail_phase;
}


void lsu::restore(unsigned int recover_lq_tail, bool recover_lq_tail_phase,
                  unsigned int recover_sq_tail, bool recover_sq_tail_phase) {
	/////////////////////////////
	// Restore LQ.
	/////////////////////////////

	// Restore tail state.
	lq_tail = recover_lq_tail;
	lq_tail_phase = recover_lq_tail_phase;

	// Recompute the length.
	lq_length = MOD_S((lq_size + lq_tail - lq_head), lq_size);
	if ((lq_length == 0) && (lq_tail_phase != lq_head_phase)) {
		lq_length = lq_size;
	}

	// Restore valid bits in two steps:
	// (1) Clear all valid bits.
	// (2) Set valid bits between head and tail.

	for (unsigned int i = 0; i < lq_size; i++) {
		LQ[i].valid = false;
	}

	for (unsigned int i = 0, j = lq_head; i < lq_length; i++, j = MOD_S((j+1), lq_size)) {
		LQ[j].valid = true;
	}

	/////////////////////////////
	// Restore SQ.
	/////////////////////////////

	// Restore tail state.
	sq_tail = recover_sq_tail;
	sq_tail_phase = recover_sq_tail_phase;

	// Recompute the length.
	sq_length = MOD_S((sq_size + sq_tail - sq_head), sq_size);
	if ((sq_length == 0) && (sq_tail_phase != sq_head_phase)) {
		sq_length = sq_size;
	}

	// Restore valid bits in two steps:
	// (1) Clear all valid bits.
	// (2) Set valid bits between head and tail.

	for (unsigned int i = 0; i < sq_size; i++) {
		SQ[i].valid = false;
	}

	for (unsigned int i = 0, j = sq_head; i < sq_length; i++, j = MOD_S((j+1), sq_size)) {
		SQ[j].valid = true;
	}
}


// Stores are written out to the cache/memory system at the time of commit
bool lsu::commit(bool load, bool atomic_op, bool& atomic_success, bool deactivated) {
   bool exception = false;
   atomic_success = true;

   if (load) {
      // LQ should not be empty.
      assert(lq_length > 0);

      //added for DHP
      if(!deactivated){
        // STATS
        n_load++;
        if (LQ[lq_head].stat_load_stall_disambig) {
           inc_counter(load_stall_count);
           n_stall_disambig++;
        }
        if (LQ[lq_head].stat_load_stall_miss) {
           inc_counter(load_stall_miss_count);
           n_stall_miss_l++;
        }
        if (LQ[lq_head].stat_forward) {
           inc_counter(load_forward_count);
           n_forward++;
        }
        if (LQ[lq_head].missed) {
           inc_counter(load_miss_count);
        }
      }

        // Invalidate the entry.
        LQ[lq_head].valid = false;

        // Advance the head pointer and decrement the queue length.
        lq_head = MOD_S((lq_head + 1), lq_size);
        lq_length--;

        // Detect wrap-around of lq_head and toggle its phase bit accordingly.
        if (lq_head == 0) {
           lq_head_phase = !lq_head_phase;
        }
   }
   else {
      // SQ should not be empty.
      assert(sq_length > 0);

      // Confirm the passed-in atomic flag matches that of the store's entry.
      assert(atomic_op == SQ[sq_head].amo);
    
      // If this is a store-conditional instruction and its load reservation has been lost, don't send the store to memory.
      //added for DHP
      if(!deactivated){
         if (atomic_op && (proc->get_state()->load_reservation != SQ[sq_head].addr)) {
            atomic_success = false;
         }
         else {
            // Try to catch store exceptions.
            try {
               // Commit store data.
               switch (SQ[sq_head].size) {
                  case 1:
                     mmu->store_uint8(SQ[sq_head].addr, SQ[sq_head].value);
                     break;                            
                  case 2:                             
                     mmu->store_uint16(SQ[sq_head].addr, SQ[sq_head].value);
                     break;                            
                  case 4:                             
                     mmu->store_uint32(SQ[sq_head].addr, SQ[sq_head].value);
                     break;                            
                  case 8:                             
                     mmu->store_uint64(SQ[sq_head].addr, SQ[sq_head].value);
                     break;
                  default:
                     assert(0);
                     break;
               }
            } 
            catch (mem_trap_t& t) {
               exception = true;
               assert(t.cause() == CAUSE_FAULT_STORE || t.cause() == CAUSE_MISALIGNED_STORE);
               proc->PAY.buf[SQ[sq_head].pay_index].trap.post(t);
            }
         }

         // STATS
         n_store++;
         if (SQ[sq_head].missed) {
            inc_counter(store_miss_count);
         }
      }

      // Invalidate the entry.
      SQ[sq_head].valid = false;
  
      // Advance the head pointer and decrement the queue length.
      sq_head = MOD_S((sq_head + 1), sq_size);
      sq_length--;
  
      // Detect wrap-around of sq_head and toggle its phase bit accordingly.
      if (sq_head == 0) {
         sq_head_phase = !sq_head_phase;
      }
   }

   return(exception);
}


void lsu::flush() {
	// Flush LQ.
	lq_head = 0;
	lq_head_phase = false;
	lq_tail = 0;
	lq_tail_phase = false;
	lq_length = 0;

	for (unsigned int i = 0; i < lq_size; i++) {
		LQ[i].valid = false;
	}

	// Flush SQ.
	sq_head = 0;
	sq_head_phase = false;
	sq_tail = 0;
	sq_tail_phase = false;
	sq_length = 0;

	for (unsigned int i = 0; i < sq_size; i++) {
		SQ[i].valid = false;
	}
}


void lsu::copy_mem(char** master_mem_table) {
	//for (unsigned int i = 0; i < MEMORY_TABLE_SIZE; i++) {
	//	if (master_mem_table[i]) {
	//		if (!mem_table[i]) {
	//			mem_table[i] = mem_newblock();
	//		}
	//	}
	//	else {
	//		if (mem_table[i]) {
	//			free(mem_table[i]);
	//			mem_table[i] = (char*)NULL;
	//		}
	//	}
	//}
}


// STATS
void lsu::dump_stats(FILE* fp) {
	fprintf(fp, "LSU MEASUREMENTS-----------------------------------\n");

	fprintf(fp, "LOADS (retired)\n");
	fprintf(fp, "  loads            = %d\n", n_load);
	fprintf(fp, "  disambig. stall  = %d (%.2f%%)\n",
	        n_stall_disambig,
	        100.0*(double)n_stall_disambig/(double)n_load);
	fprintf(fp, "  forward          = %d (%.2f%%)\n",
	        n_forward,
	        100.0*(double)n_forward/(double)n_load);
	fprintf(fp, "  miss stall       = %d (%.2f%%)\n",
	        n_stall_miss_l,
	        100.0*(double)n_stall_miss_l/(double)n_load);

	fprintf(fp, "STORES (retired)\n");
	fprintf(fp, "  stores           = %d\n", n_store);
	fprintf(fp, "  miss stall       = %d (%.2f%%)\n",
	        n_stall_miss_s,
	        100.0*(double)n_stall_miss_s/(double)n_store);
}


///////////////////////////////////////////////////////////////////////////


void lsu::dump_lq(pipeline_t* proc, unsigned int index,FILE* file)
{
  proc->disasm(proc->PAY.buf[LQ[index].pay_index].inst,proc->cycle,proc->PAY.buf[LQ[index].pay_index].pc,proc->PAY.buf[LQ[index].pay_index].sequence,file);
  ifprintf(logging_on,file,"lq_head %d lq_tail %d\n",     lq_head, lq_tail);
  ifprintf(logging_on,file,"is_signed  : %u\t",           LQ[index].is_signed);
  ifprintf(logging_on,file,"size       : %u\t",           LQ[index].size);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"addr_avail : %u\t",           LQ[index].addr_avail);
  ifprintf(logging_on,file,"addr       : %" PRIxreg "\t", LQ[index].addr);
  ifprintf(logging_on,file,"value_avail: %u\t",           LQ[index].value_avail);
  ifprintf(logging_on,file,"value      : %" PRIxreg "\t", LQ[index].value);
  ifprintf(logging_on,file,"value      : %" PRIreg "\t",  LQ[index].value);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"missed     : %u\t",           LQ[index].missed);
  ifprintf(logging_on,file,"resolves   : %" PRIcycle "\t",LQ[index].miss_resolve_cycle);
  ifprintf(logging_on,file,"pay_index  : %u\t",           LQ[index].pay_index);
  ifprintf(logging_on,file,"prev_st    : %u\t",           LQ[index].sq_index);
  ifprintf(logging_on,file,"prev_st_ph : %u\t",           LQ[index].sq_index_phase);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"\n");
}

void lsu::dump_sq(pipeline_t* proc, unsigned int index,FILE* file)
{
  proc->disasm(proc->PAY.buf[SQ[index].pay_index].inst,proc->cycle,proc->PAY.buf[SQ[index].pay_index].pc,proc->PAY.buf[SQ[index].pay_index].sequence,file);
  ifprintf(logging_on,file,"sq_head %d sq_tail %d\n",     sq_head, sq_tail);
  ifprintf(logging_on,file,"is_signed  : %u\t",           SQ[index].is_signed);
  ifprintf(logging_on,file,"size       : %u\t",           SQ[index].size);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"addr_avail : %u\t",           SQ[index].addr_avail);
  ifprintf(logging_on,file,"addr       : %" PRIxreg "\t", SQ[index].addr);
  ifprintf(logging_on,file,"value_avail: %u\t",           SQ[index].value_avail);
  ifprintf(logging_on,file,"value      : %" PRIxreg "\t", SQ[index].value);
  ifprintf(logging_on,file,"value      : %" PRIreg "\t",  SQ[index].value);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"missed     : %u\t",           SQ[index].missed);
  ifprintf(logging_on,file,"resolves   : %" PRIcycle "\t",SQ[index].miss_resolve_cycle);
  ifprintf(logging_on,file,"pay_index  : %u\t",           SQ[index].pay_index);
  ifprintf(logging_on,file,"prev_st    : %u\t",           SQ[index].sq_index);
  ifprintf(logging_on,file,"prev_st_ph : %u\t",           SQ[index].sq_index_phase);
  ifprintf(logging_on,file,"\n");
  ifprintf(logging_on,file,"\n");
}

///////////////////////////////////////////////////////////////////////////
