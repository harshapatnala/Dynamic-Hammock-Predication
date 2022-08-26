#include "processor.h"
#include "mmu.h"

#include "CacheClass.h"
#include "fetchunit_types.h"
#include "ic.h"


ic_t::ic_t(bool perfect,
	   mmu_t *mmu,
	   uint64_t fetch_width,
           uint64_t sets, 
	   uint64_t assoc,
	   uint64_t line_size, 
	   uint64_t hit_latency,
	   uint64_t miss_latency,
	   uint64_t num_MHSRs,
	   uint64_t miss_srv_ports,
	   uint64_t miss_srv_latency,
	   pipeline_t *proc,
	   CacheClass *L2C) {
   this->perfect = perfect;
   this->mmu = mmu;
   IC = new CacheClass(sets, assoc, line_size, hit_latency, miss_latency, num_MHSRs, miss_srv_ports, miss_srv_latency, proc, "l1_ic", L2C);
   this->line_size = line_size;
   this->fetch_width = fetch_width;

   // fetch_width: number of instructions in a full fetch bundle.
   // line_size: log2 the line size (where line size is in bytes).
 
   // The minimum line size is 4 bytes, i.e., 1 instruction.
   assert(line_size >= 2);

   // Assert that the fetch width is less than or equal to the number of instructions in a cache line.
   assert(fetch_width <= (1 << (line_size - 2)));	// The 2 is for a 4-byte instruction.
}

ic_t::~ic_t() {
}

// Inputs:
// 1. cycle: This is the current cycle.
// 2. pc: This is the start PC of the fetch bundle.
//
// Outputs:
// 1. I$ hit (the return value): True for hit, false for miss.
// 2. bundle[]: If hit, the instructions are inserted into this array.
// 3. miss_resolve_cycle: If miss, this tells the caller the cycle when the miss will be resolved.
//    ** To model the miss latency, the caller must wait until this cycle before calling lookup again with the same pc. **
bool ic_t::lookup(cycle_t cycle, uint64_t pc, fetch_bundle_t bundle[], cycle_t &miss_resolve_cycle) {
   uint64_t line1, line2;
   bool hit1, hit2;
   cycle_t resolve_cycle1, resolve_cycle2;

   //////////////////////////////////////////////////////
   // Model I$ misses.
   //////////////////////////////////////////////////////

   if (!perfect) {
      // Model an interleaved I$ with two banks: fetch two consecutive lines, starting with the line that the pc falls within.
      line1 = (pc >> line_size);
      line2 = (pc >> line_size) + 1;
      resolve_cycle1 = IC->Access(0, cycle, (line1 << line_size), false, &hit1);
      resolve_cycle2 = IC->Access(0, cycle, (line2 << line_size), false, &hit2);

      if (!hit1 || !hit2) {
         miss_resolve_cycle = MAX((hit1 ? (cycle_t)0 : resolve_cycle1), (hit2 ? (cycle_t)0 : resolve_cycle2));
         assert(miss_resolve_cycle > cycle);
         return(false);	// I$ miss, and we properly set the miss_resolve_cycle.
      }
   }

   //////////////////////////////////////////////////////
   // Get fetch_width sequential instructions.
   //////////////////////////////////////////////////////
   for (uint64_t i = 0; i < fetch_width; i++) {
      // Try fetching the instruction via the MMU.
      // Generate a "NOP with fetch exception" if the MMU reference generates an exception.
      bundle[i].exception = false;
      try {
         bundle[i].insn = (mmu->load_insn(pc)).insn;
      }
      catch (trap_t& t) {
	 bundle[i].exception = true;
         bundle[i].exception_cause = t.cause();
         bundle[i].insn = insn_t(INSN_NOP);
	 break;	// Exit loop: terminate the sequential fetch bundle at the offending instruction.
      }

      // Increment pc to get to the next sequential instruction.
      pc = INCREMENT_PC(pc);
   }

   return(true);	// I$ hit, and the miss_resolve_cycle is a dont-care.
}
