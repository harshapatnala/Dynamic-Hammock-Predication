typedef
struct {
   bool valid;                  // If true, override the real branch predictor with the perfect conditional branch predictions and indirect target in this packet.
   uint64_t cb_predictions;     // Perfect conditional branch predictions in the perfect fetch bundle.
                                // Predictions are in the same format as the output of the real multiple-branch predictor:
				// multiple two-bit counters packed into a uint64_t, one for each conditional branch in the perfect fetch bundle.
   uint64_t indirect_target;    // If the perfect fetch bundle ends in an indirect branch, this is its perfectly-predicted target.
} perfectbp_t;
