
class ras_t {
private:
	uint64_t *ras;
	uint64_t size;
	uint64_t tos;

public:
	ras_t(uint64_t size);
	~ras_t();
	void push(uint64_t x);	// a call pushes its return address onto the RAS
	uint64_t pop();		// a return pops its predicted return address from the RAS
	uint64_t peek();	// the branch prediction unit can examine the predicted return address without popping it

	// Functions to get and set the top-of-stack index, e.g., for checkpoint/restore purposes.
	uint64_t get_tos();
	void set_tos(uint64_t tos);
};
