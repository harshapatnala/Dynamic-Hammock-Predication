#include <cinttypes>
#include <cassert>

#include "processor.h"
#include "decode.h"
#include "config.h"

#include "fetchunit_types.h"
#include "bq.h"

bq_t::bq_t(uint64_t size) {
   this->size = ((size > 0) ? size : 1);
   bq = new bq_entry_t[this->size];
   head = 0;
   tail = 0;
   head_phase = false;
   tail_phase = false;
}

bq_t::~bq_t() {
}

void bq_t::push(uint64_t &pred_tag, bool &pred_tag_phase) {
   // Assert branch queue isn't full.
   assert((tail != head) || (tail_phase == head_phase));

   // Get index of entry to be pushed.
   pred_tag = tail;
   pred_tag_phase = tail_phase;

   // Increment tail to point to next free entry.
   tail++;
   if (tail == size) {
      tail = 0;
      tail_phase = !tail_phase;
   }
}

void bq_t::pop(uint64_t &pred_tag, bool &pred_tag_phase) {
   // Assert branch queue isn't empty.
   assert((head != tail) || (head_phase != tail_phase));

   // Get index of entry to be popped.
   pred_tag = head;
   pred_tag_phase = head_phase;

   // Increment head pointer.
   head++;
   if (head == size) {
      head = 0;
      head_phase = !head_phase;
   }
}

void bq_t::rollback(uint64_t pred_tag, bool pred_tag_phase, bool do_checks) {
   // Check that pred_tag is between: [0, size).
   assert(pred_tag < size);

   if (do_checks) {
      // Check that pred_tag is logically between: [head, tail).
      if (head == tail) {
         // Assert branch queue isn't empty. Can't have rollbacks when there are no branches present.
         assert(head_phase != tail_phase);

         if (pred_tag_phase == head_phase)
            assert(pred_tag >= head);
         else
            assert(pred_tag < head);
      }
      else if (head < tail) {
         // Head and tail phases should match in this case, by the way.
         assert(head_phase == tail_phase);

         // Assert pred_tag is between: [head, tail).
         assert((pred_tag >= head) && (pred_tag < tail));
         assert(pred_tag_phase == head_phase);
      }
      else { // (head > tail)
         // Head and tail phases should NOT match in this case, by the way.
         assert(head_phase != tail_phase);

         // Assert pred_tag is between: [0, tail) or [head, size).
         assert((pred_tag < tail) || (pred_tag >= head));
         if (pred_tag < tail)
            assert(pred_tag_phase == tail_phase);
         else
            assert(pred_tag_phase == head_phase);
      }
   }

   // Perform the rollback.
   tail = pred_tag;
   tail_phase = pred_tag_phase;
}

void bq_t::mark(uint64_t &pred_tag, bool &pred_tag_phase) {
   // Return the current tail so that the user may "mark" (record) it.
   pred_tag = tail;
   pred_tag_phase = tail_phase;
}

uint64_t bq_t::flush() {
   // Make the branch queue empty by setting the tail to the head.
   tail = head;
   tail_phase = head_phase;

   // Return the index of the head entry.
   return(head);
}
