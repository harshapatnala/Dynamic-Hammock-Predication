#include "pipeline.h"


void pipeline_t::fetch() {

   // The Fetch stage is sub-pipelined into two sub-stages: Fetch1 and Fetch2.
   // Like all pipeline stages, call them in reverse order.
   //
   // Fetch2 checks for a "misfetched bundle".
   // This check gates the clocking of Fetch1.
   // Fetch2 returns "true" if its fetch bundle is ok.
   // Fetch2 returns "false" if its fetch bundle is malformed (due to BTB misses for a fetch bundle supplied by the instruction cache).
   //
   // In the case of a misfetched bundle, Fetch2 discards its bundle and rolls-back Fetch1's state so that Fetch1 is poised to
   // repredict the bundle.  We mustn't clock the Fetch1 stage in this cycle because we need to model the fact that Fetch1 is
   // discarding the bundle that it would have fetched after the misfetched bundle. We model this discard by not clocking Fetch1 this cycle.
   // It will get clocked in the next cycle and that models repredicting the misfetched bundle in the next cycle.

   if (FetchUnit->fetch2(DECODE))	// The DECODE[] pipeline register is passed in so that the Fetch2 stage can advance its bundle to the Decode stage.
      FetchUnit->fetch1(cycle);		// The current cycle is passed in so that the Fetch1 stage can model the cycle at which an instruction cache miss resolves.

}			// fetch()
