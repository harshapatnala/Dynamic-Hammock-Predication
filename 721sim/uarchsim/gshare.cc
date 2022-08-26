#include <cinttypes>
#include "gshare.h"

gshare_index_t::gshare_index_t(uint64_t pc_length, uint64_t bhr_length) {
   // Global branch history register.
   bhr = 0;
   bhr_msb = ((1 << bhr_length) >> 1);

   // Parameters for index generation.
   pc_mask = ((1 << pc_length) - 1);
   if (pc_length > bhr_length) {
      bhr_shamt = (pc_length - bhr_length);
      size = (1 << pc_length);
   }
   else {
      bhr_shamt = 0;
      size = (1 << bhr_length);
   }
}

gshare_index_t::~gshare_index_t() {
}

uint64_t gshare_index_t::table_size() {
   return(size);
}

// Function to generate gshare index, using the speculative BHR within this class.
uint64_t gshare_index_t::index(uint64_t pc) {
   return( ((pc >> 2) & pc_mask) ^ (bhr << bhr_shamt) );
}

// Function to generate gshare index, using a previously recorded BHR for predictor updates.
uint64_t gshare_index_t::index(uint64_t pc, uint64_t commit_bhr) {
   return( ((pc >> 2) & pc_mask) ^ (commit_bhr << bhr_shamt) );
}

// Function to update bhr.
void gshare_index_t::update_bhr(bool taken) {
   bhr = ((bhr >> 1) | (taken ? bhr_msb : 0));
}

// Function to update a user-provided bhr.
uint64_t gshare_index_t::update_my_bhr(uint64_t my_bhr, bool taken) {
   return( ((my_bhr >> 1) | (taken ? bhr_msb : 0)) );
}


// Functions to get and set the bhr, e.g., for checkpoint/restore purposes.

uint64_t gshare_index_t::get_bhr() {
   return(bhr);
}

void gshare_index_t::set_bhr(uint64_t bhr) {
   this->bhr = bhr;
}
