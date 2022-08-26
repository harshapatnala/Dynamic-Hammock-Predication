
class tc_t {
private:
	// Perfect vs. real trace cache.
	bool perfect;
	mmu_t *mmu;	// need to reference the mmu if modeling a perfect trace cache

	// "m": maximum number of conditional branches in a trace.
	uint64_t max_cb;

        // "n": maximum number of instructions in a trace.
        uint64_t max_length;

public:
	tc_t(bool perfect, mmu_t *mmu, uint64_t max_cb, uint64_t max_length);
	~tc_t();
	bool lookup(uint64_t pc, uint64_t cb_predictions, uint64_t ib_predicted_target, uint64_t ras_predicted_target, fetch_bundle_t bundle[], spec_update_t *update);
};
