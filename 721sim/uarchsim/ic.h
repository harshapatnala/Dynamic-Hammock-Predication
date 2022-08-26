
class ic_t {
private:
	bool perfect;		// If true, I$ always hits.
	mmu_t *mmu;		// Currently, IC does not actually hold the instructions; it just models timing. Thus, we get instructions from the mmu.
	CacheClass *IC;		// Instruction cache.
	uint64_t line_size;	// Log2 of line size (where line size is in bytes).
	uint64_t fetch_width;	// Number of instructions in a full fetch bundle. We assert that (fetch_width == (1 << (line_size - 2))). The 2 is for a 4-byte instr.

public:
	ic_t(bool perfect, 
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
	     CacheClass *L2C);
	~ic_t();

	bool lookup(cycle_t cycle, uint64_t pc, fetch_bundle_t bundle[], cycle_t &miss_resolve_cycle);
};
