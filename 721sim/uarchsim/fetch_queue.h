#ifndef FETCH_QUEUE_H
#define FETCH_QUEUE_H

// Forward declaring pipeline_t
class pipeline_t;

class fetch_queue {
private:
  pipeline_t* proc;
	unsigned int* q;	// fetch queue
	unsigned int size;	// size of fetch queue
	unsigned int head;	// head pointer
	unsigned int tail;	// tail pointer
	unsigned int length;		// number of instructions in fetch queue

public:
	fetch_queue(unsigned int size, pipeline_t* _proc);		// constructor

	bool enough_space(unsigned int i);	// returns true if the fetch queue has enough space for i instructions
	void push(unsigned int index);		// push an instruction (its payload buffer index) into the fetch queue

	unsigned int get_length();		// returns the number of instructions in the fetch queue
	unsigned int pop();			// pop an instruction (its payload buffer index) from the fetch queue

	void flush();				// flush the fetch queue (make it empty)
};

#endif //FETCH_QUEUE_H
