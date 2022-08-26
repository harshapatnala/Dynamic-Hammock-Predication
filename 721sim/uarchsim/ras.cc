#include <cinttypes>
#include "ras.h"

ras_t::ras_t(uint64_t size) {
   this->size = ((size > 0) ? size : 1);
   ras = new uint64_t[this->size];
   tos = 0;
}

ras_t::~ras_t() {
}

// a call pushes its return address onto the RAS
void ras_t::push(uint64_t x) {
   ras[tos] = x;
   tos++;
   if (tos == size)
      tos = 0;
}

// a return pops its predicted return address from the RAS
uint64_t ras_t::pop() {
   tos = ((tos > 0) ? (tos - 1) : (size - 1));
   return(ras[tos]);
}

// the branch prediction unit can examine the predicted return address without popping it
uint64_t ras_t::peek() {
   uint64_t temp = ((tos > 0) ? (tos - 1) : (size - 1));
   return(ras[temp]);
}

// Functions to get and set the top-of-stack index, e.g., for checkpoint/restore purposes.

uint64_t ras_t::get_tos() {
   return(tos);
}

void ras_t::set_tos(uint64_t tos) {
   this->tos = tos;
}

