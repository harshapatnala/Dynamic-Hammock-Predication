// See LICENSE for license details.

#ifndef _RISCV_MMU_H
#define _RISCV_MMU_H

#include "decode.h"
#include "trap.h"
#include "common.h"
#include "config.h"
#include "processor.h"
#include "memtracer.h"
#include <vector>
#include "debug.h"

// virtual memory configuration
typedef reg_t pte_t;
const reg_t LEVELS = sizeof(pte_t) == 8 ? 3 : 2;
const reg_t PTIDXBITS = 10;
const reg_t PGSHIFT = PTIDXBITS + (sizeof(pte_t) == 8 ? 3 : 2);
const reg_t PGSIZE = 1 << PGSHIFT;
const reg_t VPN_BITS = PTIDXBITS * LEVELS;
const reg_t PPN_BITS = 8*sizeof(reg_t) - PGSHIFT;
const reg_t VA_BITS = VPN_BITS + PGSHIFT;

//struct insn_fetch_t
//{
//  insn_func_t func;
//  insn_t insn;
//};

struct icache_entry_t {
  reg_t tag;
  reg_t pad;
  insn_fetch_t data;
};

// this class implements a processor's port into the virtual memory system.
// an MMU and instruction cache are maintained for simulator performance.
class mmu_t
{
public:
  mmu_t(char* _mem, size_t _memsz);
  mmu_t(char* _mem, size_t _memsz, bool _debug_mmu);
  ~mmu_t();

  // template for functions that load an aligned value from memory
  #define load_func(type) \
    type##_t load_##type(reg_t addr) __attribute__((always_inline)) { \
      void* paddr = translate(addr, sizeof(type##_t), false, false); \
      return *(type##_t*)paddr; \
    }

  #ifdef RISCV_MICRO_CHECKER
    #undef load_func
    // template for functions that load an aligned value from memory
    #define load_func(type) \
      type##_t load_##type(reg_t addr) __attribute__((always_inline)) { \
        /* Must push addr before translation as translation may cause exception*/ \
        if((!debug_mmu) && proc->get_checker()){ proc->get_pipe()->push_address_actual(addr, MSRC_OPERAND, proc->get_state()->pc, 0, 0); } \
        void* paddr = translate(addr, sizeof(type##_t), false, false); \
        type##_t rdata = *(type##_t*)paddr; \
        if((!debug_mmu) && proc->get_checker()){ proc->get_pipe()->push_load_data_actual(addr, MSRC_OPERAND, proc->get_state()->pc, (reg_t)rdata, 0); } \
        return rdata; \
      }
  #endif

  // load value from memory at aligned address; zero extend to register width
  load_func(uint8)
  load_func(uint16)
  load_func(uint32)
  load_func(uint64)

  // load value from memory at aligned address; sign extend to register width
  load_func(int8)
  load_func(int16)
  load_func(int32)
  load_func(int64)

  //uint64_t load_uint64_htif(reg_t addr) __attribute__((always_inline)) {
  //  void* paddr = translate(addr, sizeof(uint64_t), false, false);
  //  return *(uint64_t*)paddr;
  //}

  // template for functions that store an aligned value to memory
  #define store_func(type) \
    void store_##type(reg_t addr, type##_t val) { \
      void* paddr = translate(addr, sizeof(type##_t), true, false); \
      *(type##_t*)paddr = val; \
      /*fprintf(stderr,"Storing addr 0x%" PRIxreg " paddr 0x%" PRIxreg "\n",addr,(reg_t)paddr);*/  \
    }

  #ifdef RISCV_MICRO_CHECKER
    #undef store_func
    // template for functions that load an aligned value from memory
    #define store_func(type) \
      void store_##type(reg_t addr, type##_t val) { \
        /* Only push addr and data if this is not and HTIF port to MMU and this is the ISA sim MMU*/  \
        /* Must push addr before translation as translation may cause exception*/ \
        if((!debug_mmu) && proc->get_checker()){ proc->get_pipe()->push_address_actual(addr, MDST_OPERAND, proc->get_state()->pc, 0, 0); } \
        void* paddr = translate(addr, sizeof(type##_t), true, false); \
        *(type##_t*)paddr = val; \
        /* Only push addr and data if this is not and HTIF port to MMU and this is the ISA sim MMU*/  \
        if((!debug_mmu) && proc->get_checker()){ proc->get_pipe()->push_store_data_actual(addr, MDST_OPERAND, proc->get_state()->pc, *(type##_t*)paddr, 0); }  \
      }
  #endif


  // store value to memory at aligned address
  store_func(uint8)
  store_func(uint16)
  store_func(uint32)
  store_func(uint64)

  //void store_uint64_htif(reg_t addr, uint64_t val) {
  //  void* paddr = translate(addr, sizeof(uint64_t), true, false);
  //  *(uint64_t*)paddr = val;
  //  //fprintf(stderr,"Storing addr 0x%" PRIxreg " paddr 0x%" PRIxreg "\n",addr,(reg_t)paddr);
  //}

  static const reg_t ICACHE_ENTRIES = 1024;

  inline size_t icache_index(reg_t addr)
  {
    // for instruction sizes != 4, this hash still works but is suboptimal
    return (addr / 4) % ICACHE_ENTRIES;
  }

  // load instruction from memory at aligned address.
  icache_entry_t* access_icache(reg_t addr) __attribute__((always_inline))
  {
    reg_t idx = icache_index(addr);
    icache_entry_t* entry = &icache[idx];
    if (likely(entry->tag == addr))
      return entry;

    bool rvc = false; // set this dynamically once RVC is re-implemented
    char* iaddr = (char*)translate(addr, rvc ? 2 : 4, false, true);
    insn_bits_t insn = *(uint16_t*)iaddr;

    if (unlikely(insn_length(insn) == 2)) {
      insn = (int16_t)insn;
    } else if (likely(insn_length(insn) == 4)) {
      if (likely((addr & (PGSIZE-1)) < PGSIZE-2))
        insn |= (insn_bits_t)*(int16_t*)(iaddr + 2) << 16;
      else
        insn |= (insn_bits_t)*(int16_t*)translate(addr + 2, 2, false, true) << 16;
    } else if (insn_length(insn) == 6) {
      insn |= (insn_bits_t)*(int16_t*)translate(addr + 4, 2, false, true) << 32;
      insn |= (insn_bits_t)*(uint16_t*)translate(addr + 2, 2, false, true) << 16;
    } else {
      static_assert(sizeof(insn_bits_t) == 8, "insn_bits_t must be uint64_t");
      insn |= (insn_bits_t)*(int16_t*)translate(addr + 6, 2, false, true) << 48;
      insn |= (insn_bits_t)*(uint16_t*)translate(addr + 4, 2, false, true) << 32;
      insn |= (insn_bits_t)*(uint16_t*)translate(addr + 2, 2, false, true) << 16;
    }

    insn_fetch_t fetch = {proc->decode_insn(insn), insn};
    icache[idx].tag = addr;
    icache[idx].data = fetch;

    reg_t paddr = iaddr - mem;
    if (!tracer.empty() && tracer.interested_in_range(paddr, paddr + 1, false, true))
    {
      icache[idx].tag = -1;
      tracer.trace(paddr, 1, false, true);
    }
    return &icache[idx];
  }

  inline insn_fetch_t load_insn(reg_t addr)
  {
    return access_icache(addr)->data;
  }

  void set_processor(processor_t* p) { proc = p; flush_tlb(); }

  void flush_tlb();
  void flush_icache();

  void register_memtracer(memtracer_t*);

private:
  char* mem;
  size_t memsz;
  processor_t* proc;
  memtracer_list_t tracer;

  bool debug_mmu; //Set to true if this is a debug MMU

  // implement an instruction cache for simulator performance
  icache_entry_t icache[ICACHE_ENTRIES];

  // implement a TLB for simulator performance
  static const reg_t TLB_ENTRIES = 256;
  char* tlb_data[TLB_ENTRIES];
  reg_t tlb_insn_tag[TLB_ENTRIES];
  reg_t tlb_load_tag[TLB_ENTRIES];
  reg_t tlb_store_tag[TLB_ENTRIES];

  // finish translation on a TLB miss and upate the TLB
  void* refill_tlb(reg_t addr, reg_t bytes, bool store, bool fetch);

  // perform a page table walk for a given virtual address
  pte_t walk(reg_t addr);

  // translate a virtual address to a physical address
  void* translate(reg_t addr, reg_t bytes, bool store, bool fetch)
    __attribute__((always_inline))
  {
    reg_t idx = (addr >> PGSHIFT) % TLB_ENTRIES;
    reg_t expected_tag = addr >> PGSHIFT;
    reg_t* tags = fetch ? tlb_insn_tag : store ? tlb_store_tag :tlb_load_tag;
    reg_t tag = tags[idx];
    void* data = tlb_data[idx] + addr;

    if (unlikely(addr & (bytes-1)))
      store ? throw trap_store_address_misaligned(addr) :
      fetch ? throw trap_instruction_address_misaligned(addr) :
      throw trap_load_address_misaligned(addr);

    if (likely(tag == expected_tag))
      return data;

    return refill_tlb(addr, bytes, store, fetch);
  }
  
  friend class processor_t;
};

#endif
