// See LICENSE for license details.

#include "pipeline.h"
#include "extension.h"
#include "common.h"
#include "config.h"
#include "sim.h"
#include "htif.h"
#include "disasm.h"
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <climits>
#include <stdexcept>
#include <algorithm>
#include <sys/stat.h>
#include "parameters.h"
#include <ctime>

#undef STATE
#define STATE state

static unsigned int count_bits32(unsigned int val)
{
  unsigned int count = 0;
  fprintf(stderr,"Counting bits for 0x%x\n",val);
  for(unsigned int i=0;i<32;i++)
  {
    //Mask each bit and increment count if masked value not 0
    count = (val & (1<<i)) ? count + 1: count;
  }
  return count;
}

static void print_cache_config(FILE *fp, unsigned int sets, unsigned int assoc, unsigned int blocksize, unsigned int latency, unsigned int MHSRs) {
   unsigned int i = (sets*assoc*blocksize);
   if ((i >> 20) > 0)
      fprintf(fp, "   %d MB, ", (i >> 20));
   else if ((i >> 10) > 0)
      fprintf(fp, "   %d KB, ", (i >> 10));
   else
      fprintf(fp, "   %d B, ", i);

   if (assoc == 1)
      fprintf(fp, "direct-mapped, ");
   else
      fprintf(fp, "%d-way set-associative, ", assoc);

   fprintf(fp, "%d B block size\n", blocksize);

   fprintf(fp, "   hit latency = %d cycles\n", latency);
   fprintf(fp, "   MHSRs = %d\n", MHSRs);
}


pipeline_t::pipeline_t(
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
):
  processor_t(_sim,_mmu,_id),
  statsModule(this),
  FQ(fq_size,this),
  IQ(iq_size,iq_num_parts,this),
  LSU(lq_size, sq_size, Tid, _mmu, this)
{
  unsigned int i, j, ex_depth;
  // Initialize the thread id.
  this->Tid = _id;

  // Initialize simulator time:
  cycle = 0;
  sequence = 0;

  // Initialize number of retired instructions.
  num_insn = 0;
  num_insn_split = 0;


  /////////////////////////////////////////////////////////////
  // Pipeline widths.
  /////////////////////////////////////////////////////////////
  this->fetch_width = fetch_width;
  this->dispatch_width = dispatch_width;
  this->issue_width = issue_width;
  this->retire_width = retire_width;

  #ifdef RISCV_MICRO_DEBUG
    mkdir("micros_log",S_IRWXU);
    this->fetch_log     = fopen("micros_log/fetch.log", "w")  ;
    this->decode_log    = fopen("micros_log/decode.log", "w")  ;
    this->rename_log    = fopen("micros_log/rename.log", "w")  ;
    this->dispatch_log  = fopen("micros_log/dispatch.log", "w")  ;
    this->issue_log     = fopen("micros_log/issue.log", "w")  ;
    this->regread_log   = fopen("micros_log/regread.log", "w")  ;
    this->execute_log   = fopen("micros_log/execute.log", "w")  ;
    this->lsu_log       = fopen("micros_log/lsu.log", "w")  ;
    this->wback_log     = fopen("micros_log/wback.log", "w")  ;
    this->retire_log    = fopen("micros_log/retire.log", "w")  ;
    this->program_log   = fopen("micros_log/program.log", "w")  ;
    this->cache_log     = fopen("micros_log/cache.log", "w")  ;
  #endif

  /////////////////////////////////////////////////////////////
  // Statistics unit
  /////////////////////////////////////////////////////////////
  time_t now = time(0);
  tm *ltm = localtime(&now);
  char tempstr[1024];

  // stats must be constructed first as other classes use them
  this->stats = &statsModule;
  #define OPEN_LOG_FILE(x) (sprintf(tempstr, "%s.%d-%02d-%02d.%02d:%02d:%02d.log", (x),               \
                                             (ltm->tm_year - 100), (1 + ltm->tm_mon), (ltm->tm_mday), \
                                             (ltm->tm_hour), (ltm->tm_min), (ltm->tm_sec)),           \
                                             fopen(tempstr, "w"))
  this->stats_log = OPEN_LOG_FILE("stats");
  this->phase_log = OPEN_LOG_FILE("phase");
  #undef OPEN_LOG_FILE
  stats->set_log_files(stats_log, phase_log);
  stats->set_phase_interval("commit_count", phase_interval);

  /////////////////////////////////////////////////////////////
  // Unified L2 cache.
  /////////////////////////////////////////////////////////////

  if(L2_PRESENT){
    L2C = new CacheClass( L2_SETS,
                        L2_ASSOC,
                        L2_LINE_SIZE,
                        L2_HIT_LATENCY,
                        L2_MISS_LATENCY,
                        L2_NUM_MHSRs,
                        L2_MISS_SRV_PORTS,
                        L2_MISS_SRV_LATENCY,
                        this,
                        "l2_c",
                        NULL);
  } else {
    L2C = NULL;
  }

  /////////////////////////////////////////////////////////////
  // Fetch unit.
  /////////////////////////////////////////////////////////////
 
  FetchUnit = new fetchunit_t(fetch_width,
			      COND_BRANCH_PRED_PER_CYCLE,
			      BTB_ENTRIES,
			      BTB_ASSOC,
			      CBP_PC_LENGTH, CBP_BHR_LENGTH,
			      IBP_PC_LENGTH, IBP_BHR_LENGTH,
			      RAS_SIZE,
			      BQ_SIZE,
			      ENABLE_TRACE_CACHE,
			      PERFECT_TRACE_CACHE,
			      PERFECT_BRANCH_PRED,
			      PERFECT_ICACHE,
			      L1_IC_SETS,
			      L1_IC_ASSOC,
			      L1_IC_LINE_SIZE,
			      L1_IC_HIT_LATENCY,
			      L1_IC_MISS_LATENCY,
			      L1_IC_NUM_MHSRs,
			      L1_IC_MISS_SRV_PORTS,
			      L1_IC_MISS_SRV_LATENCY,
			      L2C,   // pointer to L2 cache
			      _mmu,  // pointer to mmu
			      this,  // pointer to pipeline_t
			      &PAY, // pointer to PAY
            H_file); //pointer to DHP file ----------------------- ADDED CODE ---------------------  
  /////////////////////////////////////////////////////////////
  // Pipeline register between the Fetch and Decode Stages.
  /////////////////////////////////////////////////////////////
  DECODE = new pipeline_register[fetch_width];
  /////////////////////////////////////////////////////////////
  // Pipeline register between the Rename1 and Rename2
  // sub-stages (within the Rename Stage).
  /////////////////////////////////////////////////////////////
  RENAME2 = new pipeline_register[dispatch_width];

  ////////////////////////////////////////////////////////////
  // Set up the register renaming modules.
  ////////////////////////////////////////////////////////////
  REN = new renamer(NXPR+NFPR, (NXPR + NFPR + rob_size), num_chkpts);

  /////////////////////////////////////////////////////////////
  // Pipeline register between the Rename and Dispatch Stages.
  /////////////////////////////////////////////////////////////
  DISPATCH = new pipeline_register[dispatch_width];

  /////////////////////////////////////////////////////////////
  // Execution Lanes.
  /////////////////////////////////////////////////////////////
  Execution_Lanes = new lane[issue_width];

  for (i = 0; i < issue_width; i++) {
    ex_depth = 0;
    for (j = 0; j < (unsigned int)NUMBER_FU_TYPES; j++) {
       if (fu_lane_matrix[j] & (1 << i)) {
          if (ex_depth == 0) {
	     ex_depth = fu_lat[j];
	  }
          else if (ex_depth != fu_lat[j]) {
	     printf("Error: you tried to configure multiple FU types of different latencies to the same lane.\n");
	     printf("Adjust fu_lane_matrix[] and/or fu_lat[] so that a given lane has a single latency for all its FU types.\n");
	     exit(-1);
          }
       }
    }
    if (ex_depth > 0) {
       Execution_Lanes[i].init(ex_depth);
    }
    else {
       printf("Error: no FU type is configured to lane %d.  Therefore, this lane's latency is unknown.\n", i);
       printf("Adjust fu_lane_matrix[] such that at least one FU type is configured to lane %d.\n", i);
       exit(-1);
    }
  }

  for (i = 0; i < (unsigned int)NUMBER_FU_TYPES; i++) {
    this->fu_lane_matrix[i] = fu_lane_matrix[i];
    this->fu_lane_ptr[i] = 0;
  }

  /////////////////////////////////////////////////////////////
  // Load-Store Unit.
  /////////////////////////////////////////////////////////////

  LSU.set_l2_cache(L2C);


  // Declare and set the various knobs in the knobs database.
  // These will be printed in the stats.log file at the end of the run.

  // Print configuration to output file.

  fprintf(stats_log, "\n=== PERFECT FLAGS ===============================================================\n\n");
  fprintf(stats_log, "PERFECT_BRANCH_PRED = %d\n", (PERFECT_BRANCH_PRED ? 1 : 0));
  fprintf(stats_log, "PERFECT_DCACHE      = %d\n", (PERFECT_DCACHE ? 1 : 0));
  fprintf(stats_log, "PERFECT_ICACHE      = %d\n", (PERFECT_ICACHE ? 1 : 0));
  fprintf(stats_log, "PERFECT_TRACE_CACHE = %d\n", (PERFECT_TRACE_CACHE ? 1 : 0));
  fprintf(stats_log, "ORACLE_DISAMBIG     = %d\n", (ORACLE_DISAMBIG ? 1 : 0));

  fprintf(stats_log, "\n=== STRUCTURES AND POLICIES =====================================================\n\n");
  fprintf(stats_log, "FETCH QUEUE = %d\n", fq_size);
  fprintf(stats_log, "RENAMER:\n");
  fprintf(stats_log, "   ACTIVE LIST = %d\n", rob_size);
  fprintf(stats_log, "   PHYSICAL REGISTER FILE = %d\n", (NXPR + NFPR + rob_size));
  fprintf(stats_log, "   BRANCH CHECKPOINTS = %d\n", num_chkpts);
  fprintf(stats_log, "SCHEDULER:\n");
  fprintf(stats_log, "   ISSUE QUEUE = %d\n", iq_size);
  fprintf(stats_log, "   PARTITIONS = %d\n", iq_num_parts);
  fprintf(stats_log, "   PRESTEER = %d\n", (PRESTEER ? 1 : 0));
  fprintf(stats_log, "   IDEAL AGE-BASED = %d\n", (IDEAL_AGE_BASED ? 1 : 0));
  fprintf(stats_log, "LOAD/STORE UNIT:\n");
  fprintf(stats_log, "   LOAD QUEUE = %d\n", lq_size);
  fprintf(stats_log, "   STORE QUEUE = %d\n", sq_size);
  fprintf(stats_log, "   SPECULATIVE DISAMBIGUATION = %d\n", SPEC_DISAMBIG);
  fprintf(stats_log, "   USE STICKY-BIT MEMORY DEPENDENCE PREDICTOR = %d\n", MEM_DEP_PRED);

  fprintf(stats_log, "\n=== PIPELINE STAGE WIDTHS =======================================================\n\n");
  fprintf(stats_log, "FETCH WIDTH = %d\n", fetch_width);
  fprintf(stats_log, "DISPATCH WIDTH = %d\n", dispatch_width);
  fprintf(stats_log, "ISSUE WIDTH = %d\n", issue_width);
  fprintf(stats_log, "RETIRE WIDTH = %d\n", retire_width);

  fprintf(stats_log, "\n=== EXECUTION LANES =============================================================\n\n");
  fprintf(stats_log, "        |latency |   BR   |   LS   |  ALU_S |  ALU_C | LS_FP  | ALU_FP |  MTF   |\n");
  fprintf(stats_log, "        +--------+--------+--------+--------+--------+--------+--------+--------+\n");
  for (i = 0; i < issue_width; i++) {
    fprintf(stats_log, "lane %d  |   %2d   ", i, Execution_Lanes[i].ex_depth);
    fprintf(stats_log, "|   %c    ", ((fu_lane_matrix[FU_BR] & (1 << i)) ? 'x' : ' '));
    fprintf(stats_log, "|   %c    ", ((fu_lane_matrix[FU_LS] & (1 << i)) ? 'x' : ' '));
    fprintf(stats_log, "|   %c    ", ((fu_lane_matrix[FU_ALU_S] & (1 << i)) ? 'x' : ' '));
    fprintf(stats_log, "|   %c    ", ((fu_lane_matrix[FU_ALU_C] & (1 << i)) ? 'x' : ' '));
    fprintf(stats_log, "|   %c    ", ((fu_lane_matrix[FU_LS_FP] & (1 << i)) ? 'x' : ' '));
    fprintf(stats_log, "|   %c    ", ((fu_lane_matrix[FU_ALU_FP] & (1 << i)) ? 'x' : ' '));
    fprintf(stats_log, "|   %c    |\n", ((fu_lane_matrix[FU_MTF] & (1 << i)) ? 'x' : ' '));
  }

  fprintf(stats_log, "\n=== MEMORY HIERARCHY ============================================================\n\n");

  fprintf(stats_log, "L1 I$:\n");
  print_cache_config(stats_log, L1_IC_SETS, L1_IC_ASSOC, (1<<L1_IC_LINE_SIZE), L1_IC_HIT_LATENCY, L1_IC_NUM_MHSRs);
  if (!L2_PRESENT) fprintf(stats_log, "   miss latency = %d cycles\n", L1_IC_MISS_LATENCY);

  fprintf(stats_log, "L1 D$:\n");
  print_cache_config(stats_log, L1_DC_SETS, L1_DC_ASSOC, (1<<L1_DC_LINE_SIZE), L1_DC_HIT_LATENCY, L1_DC_NUM_MHSRs);
  if (!L2_PRESENT) fprintf(stats_log, "   miss latency = %d cycles\n", L1_DC_MISS_LATENCY);

  if (L2_PRESENT) {
     fprintf(stats_log, "L2$:\n");
     print_cache_config(stats_log, L2_SETS, L2_ASSOC, (1<<L2_LINE_SIZE), L2_HIT_LATENCY, L2_NUM_MHSRs);
     fprintf(stats_log, "   miss latency = %d cycles\n", L2_MISS_LATENCY);
  }

  fprintf(stats_log, "\n=== BRANCH PREDICTOR ============================================================\n\n");

  fprintf(stats_log, "BQ_SIZE = %d\n", BQ_SIZE);
  fprintf(stats_log, "BTB_ENTRIES = %d\n", BTB_ENTRIES);
  fprintf(stats_log, "BTB_ASSOC = %d\n", BTB_ASSOC);
  fprintf(stats_log, "RAS_SIZE = %d\n", RAS_SIZE);
  fprintf(stats_log, "COND_BRANCH_PRED_PER_CYCLE = %d\n", COND_BRANCH_PRED_PER_CYCLE);
  fprintf(stats_log, "CBP_PC_LENGTH = %d\n", CBP_PC_LENGTH);
  fprintf(stats_log, "CBP_BHR_LENGTH = %d\n", CBP_BHR_LENGTH);
  fprintf(stats_log, "IBP_PC_LENGTH = %d\n", IBP_PC_LENGTH);
  fprintf(stats_log, "IBP_BHR_LENGTH = %d\n", IBP_BHR_LENGTH);
  fprintf(stats_log, "ENABLE_TRACE_CACHE = %d\n", (ENABLE_TRACE_CACHE ? 1 : 0));

  fprintf(stats_log, "\n=== END CONFIGURATION ===========================================================\n\n");

  //DECLARE_KNOB(get_stats(), ctiq_size, CTIQ_SIZE, proc);
  //DECLARE_KNOB(get_stats(), btb_size, BTB_SIZE, proc);
  //DECLARE_KNOB(get_stats(), bp_table_size, BP_TABLE_SIZE, proc);
  //DECLARE_KNOB(get_stats(), ras_size, RAS_SIZE, proc);


  // Provide each module with a pointer to the stats object
  LSU.set_stats(get_stats());
  //REN_INT->set_stats(get_stats());
//  FIX_ME
//  FetchUnit->set_stats(get_stats());

  ///////////////////////////////////////////////////
  // Set up the memory system.
  ///////////////////////////////////////////////////

  reset(true);
  mmu->set_processor(this);

}


pipeline_t::~pipeline_t()
{
  //stats->dump_knobs();
  stats->dump_counters();
  stats->dump_rates();
  stats->dump_pc_histogram();
  stats->dump_br_histogram();
#ifdef RISCV_ENABLE_HISTOGRAM
  if (histogram_enabled)
  {
    ifprintf(logging_on,stderr, "PC Histogram size:%lu\n", pc_histogram.size());
    for(auto iterator = pc_histogram.begin(); iterator != pc_histogram.end(); ++iterator) {
      ifprintf(logging_on,stderr, "%0lx %lu\n", (iterator->first << 2), iterator->second);
    }
  }
#endif

  FetchUnit->output(stats->get_counter("commit_count"), stats->get_counter("cycle_count"), stats_log);
  LSU.dump_stats(stats_log);

  #ifdef RISCV_MICRO_DEBUG
    fclose(this->fetch_log    );
    fclose(this->decode_log   );
    fclose(this->rename_log   );
    fclose(this->dispatch_log );
    fclose(this->issue_log    );
    fclose(this->regread_log  );
    fclose(this->execute_log  );
    fclose(this->lsu_log      );
    fclose(this->wback_log    );
    fclose(this->retire_log   );
    fclose(this->program_log  );
    fclose(this->cache_log    );
  #endif

  fclose(this->stats_log   );
  fclose(this->phase_log   );
}

inline void pipeline_t::update_histogram(size_t pc)
{
#ifdef RISCV_ENABLE_HISTOGRAM
  size_t idx = pc >> 2;
  pc_histogram[idx]++;
#endif
}


static void commit_log(state_t* state, reg_t pc, insn_t insn)
{
#ifdef RISCV_ENABLE_COMMITLOG
  if (state->sr & SR_EI) {
    uint64_t mask = (insn.length() == 8 ? uint64_t(0) : (uint64_t(1) << (insn.length() * 8))) - 1;
    if (state->log_reg_write.addr) {
      ifprintf(logging_on,stderr, "0x%016" PRIx64 " (0x%08" PRIx64 ") %c%2" PRIu64 " 0x%016" PRIx64 "\n",
              pc,
              insn.bits() & mask,
              state->log_reg_write.addr & 1 ? 'f' : 'x',
              state->log_reg_write.addr >> 1,
              state->log_reg_write.data);
    } else {
      ifprintf(logging_on,stderr, "0x%016" PRIx64 " (0x%08" PRIx64 ")\n", pc, insn.bits() & mask);
    }
  }
  state->log_reg_write.addr = 0;
#endif
}


//Scope of this function is just this file
static reg_t execute_insn(pipeline_t* p, reg_t pc, insn_fetch_t fetch)
{
  reg_t npc = fetch.func(p, fetch.insn, pc);
  commit_log(p->get_state(), pc, fetch.insn);
  p->update_histogram(pc);
  return npc;
}

//Scope of this function is just this file
static void update_timer(state_t* state, size_t instret)
{
  uint64_t count0 = (uint64_t)(uint32_t)state->count;
  state->count += instret;
  uint64_t before = count0 - state->compare;
  if (int64_t(before ^ (before + instret)) < 0)
    state->sr |= (1 << (IRQ_TIMER + SR_IP_SHIFT));
}

//Scope of this function is just this file
static size_t next_timer(state_t* state)
{
  ifprintf(logging_on,stderr,"MICRO_SIM next timer: %u\n",(state->compare - (uint32_t)state->count));
  return state->compare - (uint32_t)state->count;
}

bool pipeline_t::step_micro(size_t instret_limit, size_t& instret)
{
  instret = 0;
  size_t prev_instret = 0;

  //TODO: This is needed for functional simulator
  //reg_t pc = state.pc;
  mmu_t* _mmu = mmu;

  if (unlikely(!run || !instret_limit)) {
    return true;
  }
  instret_limit = std::min(instret_limit, next_timer(&state) | 1U);

  try
  {
    take_interrupt();

    if (unlikely(debug))
    {
      //TODO: If the system suddenly enters debug mode, 
      //1) Truncate execution at the AL head only once, set a flag to indicate this has already happened
      //2) If all state is maintained, this should just work.
      while (instret < instret_limit)
      {
        instret++;
        insn_fetch_t fetch = mmu->load_insn(FetchUnit->getPC());
        disasm(fetch.insn);
        FetchUnit->setPC(execute_insn(this, FetchUnit->getPC(), fetch));
        update_timer(&state, instret-prev_instret);
        prev_instret = instret;
        // If this was an idle cycle break
        if(!instret)
          break;
      }
    }
    else {

        /////////////////////////////////////////////////////////////
        // 1 cycle of Pipeline.
        /////////////////////////////////////////////////////////////

        size_t lane_number;

        unsigned int prev_commit_count = counter(commit_count);
        for (lane_number = 0; lane_number < RETIRE_WIDTH; lane_number++) {
          retire(instret);            // Retire Stage
          update_timer(&state, instret-prev_instret);
          prev_instret = instret;
          // Halt retirement if its time for an HTIF tick as this will change state
          if(instret == instret_limit)
            break;
          // Stop simulation if limit reached
          if((counter(commit_count) >= stop_amt) && use_stop_amt){
            //stats->dump_knobs();
            //stats->dump_counters();
            //stats->dump_rates();
            return true;
          }
        }
        // Increment the retired bundle count if even a single instruction retired
        if(counter(commit_count) > prev_commit_count)
          inc_counter(retired_bundle_count);

        //REN_INT->dump_al(this,PAY,2,regread_log);
        for (lane_number = 0; lane_number < ISSUE_WIDTH; lane_number++) {
          writeback(lane_number);    // Writeback Stage
        }
        load_replay();
        for (lane_number = 0; lane_number < ISSUE_WIDTH; lane_number++) {
          execute(lane_number);    // Execute Stage
        }
        for (lane_number = 0; lane_number < ISSUE_WIDTH; lane_number++) {
          register_read(lane_number);    // Register Read Stage
        }
        schedule();           // Schedule Stage
        dispatch();           // Dispatch Stage
        rename2();            // Rename Stage
        rename1();            // Rename Stage
        decode();             // Decode Stage
        //// FETCH will insert NOPs instead of fetching real instructions
        //// from cache if a fetch_exception is pending. This is to make
        //// dispatch never gets stalled due to the absense of a full bundle
        //// in the FETCH QUEUS.his is sort of like a stall.
        //if(!fetch_exception){
          fetch();            // Fetch Stage
        //}

        /////////////////////////////////////////////////////////////
        // Miscellaneous stuff that must be processed every cycle.
        /////////////////////////////////////////////////////////////

        // Go to the next simulator cycle.
        //next_cycle();
        cycle++;
        inc_counter(cycle_count);

        if(cycle > (uint64_t)logging_on_at)
          logging_on = true;

	static uint64_t grading_plateau = 1000;
	if (num_insn >= grading_plateau) {
	   INFO("GRADING PLATEAU: %lu", grading_plateau);
	   grading_plateau *= 10;
	}

        // For detecting deadlock, and monitoring progress.
        if (MOD(cycle, 0x400000) == 0) {
          INFO("(cycle = %" PRIcycle " ) num_insn = %.0f\tIPC = %.2f",
               cycle,
               (double)num_insn,
               (double)num_insn/(double)cycle);
          //stats->dump_counters();
          //stats->dump_rates();

	  static uint64_t num_insn_last_beat = 0;
	  if (num_insn == num_insn_last_beat) {
	     INFO("DEADLOCK.");
	     assert(0);
          }
	  num_insn_last_beat = num_insn;
        }

    }
  }
  //catch(mem_trap_t& t)
  //{
  //  //TODO: This can be the point of synchronization when checking is implemented.
  //  //Only the functional simulator takes the trap and then once it is done, all 
  //  //architectural state will be copied over to this processor, including CSRs etc.
  //  ifprintf(logging_on,stderr, "MICRO_SIM: Cycle %" PRIcycle ": core %3d: mem exception %s, epc 0x%016" PRIx64 " badvaddr 0x%016" PRIx64 "\n",
  //          cycle, id, t.name(), pc, t.get_badvaddr());
  //  pc = take_trap(t, pc);
  //}
  catch(trap_t& t)
  {
    //TODO: This can be the point of synchronization when checking is implemented.
    //Only the functional simulator takes the trap and then once it is done, all 
    //architectural state will be copied over to this processor, including CSRs etc.
    FetchUnit->setPC(take_trap(t, FetchUnit->getPC()));
  }
  catch(serialize_t& s) {
    ifprintf(logging_on,stderr,"MICROS: Caught serialize_t exception...........\n");
  }

  state.pc = FetchUnit->getPC();
  //update_timer(&state, instret);
  return false;
}

reg_t pipeline_t::take_trap(trap_t& t, reg_t epc)
{
  #ifdef RISCV_MICRO_DEBUG
    //TODO: Add if(debug) back
    //if (debug)
      ifprintf(logging_on,stderr, "MICRO_SIM: Cycle %" PRIcycle ": core %3d: exception %s, epc 0x%016" PRIx64 "\n",
              cycle, id, t.name(), epc);
  #endif

  // switch to supervisor, set previous supervisor bit, disable interrupts
  set_pcr(CSR_STATUS, (((state.sr & ~SR_EI) | SR_S) & ~SR_PS & ~SR_PEI) |
          ((state.sr & SR_S) ? SR_PS : 0) |
          ((state.sr & SR_EI) ? SR_PEI : 0));

  yield_load_reservation();
  state.cause = t.cause();
  state.epc = epc;
  t.side_effects(&state); // might set badvaddr etc.
  return state.evec;
}

void pipeline_t::disasm(insn_t insn)
{
  uint64_t bits = insn.bits() & ((1ULL << (8 * insn_length(insn.bits()))) - 1);
  ifprintf(logging_on,stderr, "core %3d: 0x%016" PRIx64 " (0x%08" PRIx64 ") %s\n",
          id, state.pc, bits, disassembler->disassemble(insn).c_str());
}

void pipeline_t::disasm(insn_t insn,reg_t pc)
{
  uint64_t bits = insn.bits() & ((1ULL << (8 * insn_length(insn.bits()))) - 1);
  ifprintf(logging_on,stderr, "core %3d: 0x%016" PRIx64 " (0x%08" PRIx64 ") %s\n",
          id, pc, bits, disassembler->disassemble(insn).c_str());
}

void pipeline_t::disasm(insn_t insn, reg_t pc, FILE* out)
{
  uint64_t bits = insn.bits() & ((1ULL << (8 * insn_length(insn.bits()))) - 1);
  ifprintf(logging_on,out, "Cycle %" PRIcycle ": Seq %" PRIu64 " PC 0x%016" PRIx64 " (0x%08" PRIx64 ") %s\n",
          cycle, sequence, pc, bits, disassembler->disassemble(insn).c_str());
}

void pipeline_t::disasm(insn_t insn, cycle_t cycle, reg_t pc, uint64_t seq, FILE* out)
{
  uint64_t bits = insn.bits() & ((1ULL << (8 * insn_length(insn.bits()))) - 1);
  ifprintf(logging_on,out, "Cycle %" PRIcycle ": Seq %" PRIu64 " PC 0x%016" PRIx64 " (0x%08" PRIx64 ") %s\n",
          cycle, seq, pc, bits, disassembler->disassemble(insn).c_str());
}



//void pipeline_t::copy_state_from_actual(db_t* actual) {
//
//    state_t* isa_state  = pipe.get_isa_state();
//
//   reg_t x;
//
//
//   for (unsigned int i = 0; i < NXPR; i++){
//      // Integer RF: general registers 0-31.
//      REN_INT->write(REN_INT->rename_rsrc(i), isa_state->NXPR[i]);
//      // Floating point RF: general registers 0-31.
//      REN_INT->write(REN_INT->rename_rsrc(i+NXPR), isa_state->NFPR[i]);
//   }
//
//}

void pipeline_t::copy_state_to_micro() {
   for (unsigned int i = 0; i < NXPR; i++){
      // Integer RF: general registers 0-31.
      REN->write(REN->rename_rsrc(i,NORMAL_TYPE), get_state()->XPR[i]);
      // Floating point RF: general registers 0-31.
      REN->write(REN->rename_rsrc(i+NXPR,NORMAL_TYPE), get_state()->FPR[i]);
   }

   FetchUnit->setPC(get_state()->pc);
}

uint64_t pipeline_t::get_arch_reg_value(int reg_id) { 

    return REN->read(REN->rename_rsrc(reg_id,NORMAL_TYPE));
}


uint32_t pipeline_t::get_instruction(uint64_t inst_pc){
  //TODO: handle fetch exceptions
  insn_fetch_t inst_raw = mmu->load_insn(inst_pc);
  return (uint32_t)inst_raw.insn.bits();
}

void pipeline_t::set_exception(unsigned int al_index) {
   REN->set_exception(al_index);
}

void pipeline_t::set_load_violation(unsigned int al_index) {
   REN->set_load_violation(al_index);
}

void pipeline_t::set_branch_misprediction(unsigned int al_index) {
   REN->set_branch_misprediction(al_index);
}

void pipeline_t::set_value_misprediction(unsigned int al_index) {
   REN->set_value_misprediction(al_index);
}
