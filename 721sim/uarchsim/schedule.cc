#include "pipeline.h"


void pipeline_t::schedule() {
   IQ.select_and_issue(issue_width, Execution_Lanes);	// Issue instructions from unified IQ to the Execution Lanes.
}
