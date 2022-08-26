#include<unordered_map>
#include <string>
#include <fstream>
#include <vector>
// typedef enum {
// 	 FORK_THEN=0,
// 	 FORK_ELSE=1, 
// 	 THEN_ELSE=2 
//  } cmov_type_e;

// typedef struct {
// 	//This field contains the logical destination register of the CMOV instruction.
// 	uint64_t log_reg; 
	
// 	//This field tells whether the hammock is single sided or double sided. 
// 	//If it is a single sided hammock, there is no else context(In the paper it is portion of code when condition is true). 
// 	//The branch's taken target itself is the reconvergent point.
// 	//If it is double sided hammock, there exists both else context and then context(In the paper then is the portion of code when condition is false,
// 	//or in other words the sequential PC after the branch).
// 	//So this indicates whether the CMOV instruction has to use the fork/then version (in case of single side hammock) 
// 	//or use the then/else version (in case of double side hammock).
// 	//0 --> Fork/then version (Single sided hammock)
// 	//1 --> Then/Else Version (Double Sided hammock)
// 	cmov_type_e type;  
// } cmov_entry;

typedef struct {
	uint64_t RPC; 			// Reconvergent PC of the Hammock
	bool else_valid; 		// Indicates whether Else context exists or not.
	//uint64_t EPC; 			// Start PC of Else Context. This is valid only if else_valid is true.
	uint64_t then_length; 	// Number of Instructions/Length of the Then Context.
	//uint64_t else_length; 	// Number of Instructions/Length of Else Context. This should be 0 in case of single sided hammock.
	uint64_t  num_cmovs; 	// Number of CMOVs required for this hammock region
	uint64_t* CMOV; 		// CMOV Information. Constructed based on number of CMOVs required.
} hammock_entry;


class hammock_table_t {
public:
	hammock_table_t(std::string);
	~hammock_table_t();
	bool search_hammock(uint64_t, hammock_entry&);

private:
	std::string hammock_file;
	std::ifstream file;
	std::unordered_map<uint64_t, hammock_entry> table;
};


// A BTB entry.
typedef
struct {
   // Metadata for hit/miss determination and replacement.
   bool valid;
   uint64_t tag;
   uint64_t lru;

   // Payload.
   btb_branch_type_e branch_type;
   uint64_t target;
} btb_entry_t;



class btb_t {
private:
	// The BTB has three dimensions: number of banks, number of sets per bank, and associativity (number of ways per set).
	// btb[bank][set][way]
	btb_entry_t ***btb;
	uint64_t banks;
	uint64_t sets;
	uint64_t assoc;

	uint64_t log2banks; // number of pc bits that selects the bank
	uint64_t log2sets;  // number of pc bits that selects the set within a bank

	uint64_t cond_branch_per_cycle; // "m": maximum number of conditional branches in a fetch bundle.

	////////////////////////////////////
	// Private utility functions.
	// Comments are in btb.cc.
	////////////////////////////////////

	void convert(uint64_t pc, uint64_t pos, uint64_t &btb_bank, uint64_t &btb_pc);
	bool search(uint64_t btb_bank, uint64_t btb_pc, uint64_t &set, uint64_t &way);
	void update_lru(uint64_t btb_bank, uint64_t set, uint64_t way);
	

public:
	btb_t(uint64_t num_entries, uint64_t banks, uint64_t assoc, uint64_t cond_branch_per_cycle);
	~btb_t();
        void lookup(uint64_t pc, uint64_t cb_predictions, uint64_t ib_predicted_target, uint64_t ras_predicted_target, fetch_bundle_t bundle[], spec_update_t *update);
	void update(uint64_t pc, uint64_t pos, insn_t insn);
	void invalidate(uint64_t pc, uint64_t pos);
	static btb_branch_type_e decode(insn_t insn, uint64_t pc, uint64_t &target);
	//---ADDED CODE ----
	hammock_table_t* hammock_table;
	fetch_state_e state;
	void construct_hammock_table(std::string file);
	hammock_entry entry;
	uint64_t then_count;

	std::vector <uint64_t> cmov_instructions;
	void create_cmovs(hammock_entry);
	void reset_state();
};
