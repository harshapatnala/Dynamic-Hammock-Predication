
class bq_entry_t {
public:
	// The type of branch.
	btb_branch_type_e branch_type;

	// Precise information at this point in the instruction stream.
	uint64_t precise_cb_bhr;  // Precise BHR (all prior branches included) to which we can restore the BHR of the conditional branch predictor (cb).
	uint64_t precise_ib_bhr;  // Precise BHR (all prior branches included) to which we can restore the BHR of the indirect branch predictor (ib).
	uint64_t precise_ras_tos; // Precise TOS index at this point in the instruction stream.

	// Information that was used to get the prediction.
	// A critical rule in branch prediction, is to always train the predictor entry from where the prediction was gotten (whether prediction was correct or not).
	uint64_t fetch_pc;		// PC that was used for indexing the conditional and indirect branch predictors for this prediction.
	uint64_t fetch_cb_bhr;		// BHR that was used for indexing the conditional branch predictor for this prediction.
	uint64_t fetch_ib_bhr;		// BHR that was used for indexing the indirect branch predictor for this prediction.
	uint64_t fetch_cb_pos_in_entry; // In general, the conditional branch predictor can supply a bundle of m branch predictions from a single entry.
					// This variable is the position of this prediction within the entry.

	// Branch prediction/outcome.  It is a prediction until confirmed/disconfirmed, and then it is an outcome.
	bool taken;		// For conditional branches.
	uint64_t next_pc;

	// This flag indicates whether or not the branch was mispredicted.  It is needed for measuring mispredictions at retirement.
	bool misp;
};


class bq_t {
private:
	uint64_t size;
	uint64_t head;
	uint64_t tail;
	bool head_phase;
	bool tail_phase;

public:
	// This is the branch queue.  It holds information for all outstanding branch predictions.
	// The user of this class can directly access branch queue entries, given indices from this class.
	bq_entry_t *bq;

	bq_t(uint64_t size);
	~bq_t();
	void push(uint64_t &pred_tag, bool &pred_tag_phase);
	void pop(uint64_t &pred_tag, bool &pred_tag_phase);
	void rollback(uint64_t pred_tag, bool pred_tag_phase, bool do_checks);
	void mark(uint64_t &pred_tag, bool &pred_tag_phase);
	uint64_t flush();
};

