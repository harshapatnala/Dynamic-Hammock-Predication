#include "pipeline.h"


lane::lane() {
}

void lane::init(unsigned int ex_depth) {
	// Set up the execute stage(s).
	assert(ex_depth > 0);
	this->ex_depth = ex_depth;
	ex = new pipeline_register[ex_depth];

	// The lane is initially empty of instructions.
	rr.valid = false;
	for (unsigned int i = 0; i < ex_depth; i++)
	  ex[i].valid = false;
	wb.valid = false;
}
