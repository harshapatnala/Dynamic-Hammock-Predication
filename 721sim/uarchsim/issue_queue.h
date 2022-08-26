#ifndef ISSUE_QUEUE_H
#define ISSUE_QUEUE_H

typedef struct {

	// Valid bit for the issue queue entry as a whole.
	// If true, it means an instruction occupies this issue queue entry.
	bool valid;

	// Index into the instruction payload buffer.
	unsigned int index;

	// Branches that this instruction depends on.
	uint64_t branch_mask;

	// Execution lane that this instruction wants.
	unsigned int lane_id;

	// Valid bit, ready bit, and tag of first operand (A).
	bool A_valid;		// valid bit (operand exists)
	bool A_ready;		// ready bit (operand is ready)
	unsigned int A_tag;	// physical register name

	// Valid bit, ready bit, and tag of second operand (B).
	bool B_valid;		// valid bit (operand exists)
	bool B_ready;		// ready bit (operand is ready)
	unsigned int B_tag;	// physical register name

	// Valid bit, ready bit, and tag of third operand (D).
	bool D_valid;		// valid bit (operand exists)
	bool D_ready;		// ready bit (operand is ready)
	unsigned int D_tag;	// physical register name

	// Support for ideal age-based priority.
	int prev;	// IQ index of previous-oldest instruction still in the IQ.
	int next;	// IQ index of next-oldest instruction still in the IQ.

} issue_queue_entry_t;


//Forward declaring classes
class pipeline_t;
class payload;
class stats_t;

class issue_queue {
private:
  pipeline_t* proc;
  // Needed for macro
  stats_t*    stats;
	issue_queue_entry_t* q;		// This is the issue queue.
	unsigned int size;		// Issue queue size.
	unsigned int length;			// Number of instructions in the issue queue.
 
        // Round-robin scheduling among issue queue partitions.
	unsigned int part_size;		// size of a partition
        unsigned int part_next;		// which partition has priority this cycle

	// Support for ideal age-based priority.
	int oldest;	// IQ index of oldest instruction still in the IQ.
	int youngest;	// IQ index of youngest instruction still in the IQ.

	unsigned int* fl;		// This is the list of free issue queue entries, i.e., the "free list".
	unsigned int fl_head;		// Head of issue queue's free list.
	unsigned int fl_tail;		// Tail of issue queue's free list.
	unsigned int fl_length;			// Length of issue queue's free list.

	void remove(unsigned int i);	// Remove the instruction in issue queue entry 'i' from the issue queue.


public:
	issue_queue(unsigned int size, unsigned int num_parts, pipeline_t* _proc=NULL);	// constructor
	bool stall(unsigned int bundle_inst);
	void dispatch(unsigned int index, unsigned long long branch_mask, unsigned int lane_id,
	              bool A_valid, bool A_ready, unsigned int A_tag,
	              bool B_valid, bool B_ready, unsigned int B_tag,
	              bool D_valid, bool D_ready, unsigned int D_tag);
	void wakeup(unsigned int tag);
	void select_and_issue(unsigned int num_lanes, lane* Execution_Lanes);
	void flush();
	void clear_branch_bit(unsigned int branch_ID);
	void squash(unsigned int branch_ID);
  void dump_iq(pipeline_t* proc, unsigned int index,FILE* file=stderr);
};

#endif //ISSUE_QUEUE_H
