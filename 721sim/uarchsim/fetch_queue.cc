#include "pipeline.h"


// constructor
fetch_queue::fetch_queue(unsigned int size, pipeline_t* _proc) {
	q = new unsigned int[size];
	this->size = size;
  this->proc = _proc;
	head = 0;
	tail = 0;
	length = 0;
}

// returns true if the fetch queue has enough space for i instructions
bool fetch_queue::enough_space(unsigned int i) {
	assert(length <= size);
  ifprintf(logging_on,proc->decode_log,"Fetch Queue Length: %u\n",length);
	return((size - length) >= i);
}

// push an instruction (its payload buffer index) into the fetch queue
void fetch_queue::push(unsigned int index) {
	assert(length < size);

	q[tail] = index;

	tail = MOD_S((tail + 1), size);
	length += 1;
}

// returns the number of instructions in the fetch queue
unsigned int fetch_queue::get_length() {
	return(length);
}

// pop an instruction (its payload buffer index) from the fetch queue
unsigned int fetch_queue::pop() {
	unsigned int index;

	assert(length > 0);

	index = q[head];

	head = MOD_S((head + 1), size);
	length -= 1;

	return(index);
}

// flush the fetch queue (make it empty)
void fetch_queue::flush() {
	head = 0;
	tail = 0;
	length = 0;
}
