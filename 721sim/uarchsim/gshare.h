class gshare_index_t {
private:
	// Global branch history register.
	uint64_t bhr;		// current state of global branch history register
	uint64_t bhr_msb;	// used to set the msb of the bhr

	// Parameters for index generation.
	uint64_t pc_mask;
	uint64_t bhr_shamt;

	// User can query what its predictor size should be.
	uint64_t size;

public:
	gshare_index_t(uint64_t pc_length, uint64_t bhr_length);

	~gshare_index_t();

	uint64_t table_size();

	// Functions to generate gshare index.
	uint64_t index(uint64_t pc);				// Uses the speculative BHR within this class.
	uint64_t index(uint64_t pc, uint64_t commit_bhr);	// Uses a previously recorded BHR for predictor updates.

	// Function to update bhr.
	void update_bhr(bool taken);

	// Function to update a user-provided bhr.
	uint64_t update_my_bhr(uint64_t my_bhr, bool taken);

	// Functions to get and set the bhr, e.g., for checkpoint/restore purposes.
	uint64_t get_bhr();
	void set_bhr(uint64_t bhr);
};
