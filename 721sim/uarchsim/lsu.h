#ifndef LSU_H
#define LSU_H
///////////////////////////////////////////////////////////////
// Load/Store Unit including:
// 1. Disambiguation unit in the form of a load/store queue.
// 2. Data cache (only tags, for determining hit or miss).
// 3. Committed memory state.
///////////////////////////////////////////////////////////////
//#include "CcacheClass.h"

// Single entry in the load-store queue.
typedef struct {
  bool valid;   // this entry holds an active load or store

  bool is_signed;   // signed (true) or unsigned (false)
  //bool left;    // LWL or SWL
  //bool right;   // LWR or SWR
  unsigned int size;   // 1, 2, 4, or 8 bytes
  bool amo;	// the load or store is an amo (atomic memory operation)

  bool addr_avail;  // address is available
  reg_t addr; // address
  //reg_t back_data; // background data for LWL/LWR

  bool value_avail; // value is available
  reg_t value;  // value (up to two words)

  bool missed;        // The memory block referenced by load or store is not in cache.
  cycle_t miss_resolve_cycle; // Cycle when referenced memory block will be in cache.

  // These three fields are needed for replaying stalled loads.
  unsigned int pay_index; // Index into PAY buffer.
  unsigned int sq_index;  // SQ index of stalled load.
  bool sq_index_phase;

  // Dynamic loads may be classed as "stall type" or "speculate type",
  // based on whether or not speculative memory disambiguation is enabled
  // and a prediction from the memory dependence predictor (MDP).
  bool mdp_stall;

  // STATS
  bool stat_load_stall_disambig;  // Load stalled due to unknown store address and/or value.
  bool stat_load_stall_miss;  // Load stalled due to a cache miss.
  bool stat_store_stall_miss; // Store commit stalled due to a cache miss.
  bool stat_forward;    // Load received value from store in LSQ.
} lsq_entry;


//Forward declaring classes 
class mmu_t;
class pipeline_t;
class CacheClass;
class stats_t;

class lsu {

private:

  pipeline_t* proc;
  mmu_t* mmu;
  stats_t* stats;
  //////////////////////////
  // Load Queue
  //////////////////////////
  lsq_entry* LQ;    // Loads reside in this data structure between dispatch and retirement.
  unsigned int lq_size;
  unsigned int lq_head;
  unsigned int lq_tail;
  unsigned int lq_length;

  // These extra bits enable distinguishing between a full queue versus an empty queue when head==tail.
  bool lq_head_phase;
  bool lq_tail_phase;

  //////////////////////////
  // Store Queue
  //////////////////////////
  lsq_entry* SQ;    // Stores reside in this data structure between dispatch and retirement.
  unsigned int sq_size;
  unsigned int sq_head;
  unsigned int sq_tail;
  unsigned int sq_length;

  // These extra bits enable distinguishing between a full queue versus an empty queue when head==tail.
  bool sq_head_phase;
  bool sq_tail_phase;

  //////////////////////////
  // Data Cache
  //////////////////////////
  CacheClass* DC;
  unsigned int Tid;

  //////////////////////////
  // Memory
  //////////////////////////

  //////////////////////////
  // STATS
  //////////////////////////
  // Number of retired loads that stalled for disambiguation.
  unsigned int n_stall_disambig;

  // Number of retired loads that received values from stores in LSQ.
  unsigned int n_forward;

  // Number of retired loads (l) or stores (s) that cache missed.
  unsigned int n_stall_miss_l;
  unsigned int n_stall_miss_s;

  // Number of loads and stores.
  unsigned int n_load;
  unsigned int n_store;

  //////////////////////////
  //  Private functions
  //////////////////////////

  // The core disambiguation algorithm.
  //
  // Inputs:
  // 1. LQ/SQ indices of load being checked (four pieces of info)
  //
  // Outputs:
  // 1. (return value): stall load if function returns true
  // 2. forward: forward value from prior dependent store
  // 3. store_entry: if forwarding is indicated, this identifies the store
  //
  bool disambiguate(unsigned int lq_index,
                    unsigned int sq_index, bool sq_index_phase,
                    bool& forward,
                    unsigned int& store_entry);

  // The load execution datapath.
  void execute_load(cycle_t cycle,
                    unsigned int lq_index, bool lq_index_phase,
                    unsigned int sq_index, bool sq_index_phase);

  // The path for stores to detect mispredicted loads.
  bool ld_violation(unsigned int sq_index,
                    unsigned int lq_index, bool lq_index_phase,
                    unsigned int& load_entry);

  // Allocate a chunk of memory.
  char* mem_newblock(void);


public:

  lsu(unsigned int lq_size, unsigned int sq_size, unsigned int Tid, mmu_t* _mmu, pipeline_t* _proc=NULL);   // constructor
  ~lsu();

  void set_l2_cache(CacheClass* l2_dc);

  bool stall(unsigned int bundle_load, unsigned int bundle_store);

  void dispatch(bool load, unsigned int size, bool left, bool right, bool is_signed, bool amo,
                unsigned int pay_index,
                unsigned int& lq_index, bool& lq_index_phase,
                unsigned int& sq_index, bool& sq_index_phase,
		bool mdp_stall);

  void store_addr(cycle_t cycle,
                  reg_t addr,
                  unsigned int sq_index,
                  unsigned int lq_index, bool lq_index_phase);
  void store_value(unsigned int sq_index, reg_t value);

  bool load_addr(cycle_t cycle,
                 reg_t addr,
                 unsigned int lq_index, bool lq_index_phase,
                 unsigned int sq_index, bool sq_index_phase,
                 //reg_t back_data, // LWL/LWR
                 reg_t& value);
  bool load_unstall(cycle_t cycle, unsigned int& pay_index, reg_t& value);

  void checkpoint(unsigned int& chkpt_lq_tail, bool& chkpt_lq_tail_phase,
                  unsigned int& chkpt_sq_tail, bool& chkpt_sq_tail_phase);
  void restore(unsigned int recover_lq_tail, bool recover_lq_tail_phase,
               unsigned int recover_sq_tail, bool recover_sq_tail_phase);

  bool commit(bool load, bool atomic_op, bool& atomic_success, bool deactivted);

  void flush();

  void copy_mem(char** master_mem_table);

  // STATS
  void set_stats(stats_t* _stats){this->stats = _stats;}
  void dump_stats(FILE* fp);

  void dump_lq(pipeline_t* proc, unsigned int index,FILE* file=stderr);
  void dump_sq(pipeline_t* proc, unsigned int index,FILE* file=stderr);
};

#endif //LSU_H
