#ifndef LANE_H
#define LANE_H

class lane {
public:
	pipeline_register rr;	// pipeline register of Register Read Stage
	pipeline_register *ex;	// pipeline register(s) of Execute Stage
	pipeline_register wb;	// pipeline register of Writeback Stage

	unsigned int ex_depth;  // number of sub-stages in the Execute Stage

	lane();	// constructor
	void init(unsigned int ex_depth);
};

#endif //LANE_H
