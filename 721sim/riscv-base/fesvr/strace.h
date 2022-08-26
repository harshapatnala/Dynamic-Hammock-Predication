//
// Created by s117 on 10/12/20.
//

#ifndef RISCV_ISA_SIM_STRACE_H
#define RISCV_ISA_SIM_STRACE_H

#include <fstream>
#include <iostream>
#include <cassert>
#include <cinttypes>
#include "base64.h"

#define PASS_PARAM(p) #p, p

class strace {
public:
  strace() = default;

  ~strace() {
    if (m_output_file) {
      fflush(m_output_file);
      fclose(m_output_file);
    }
  }

  void enable(const char *output_path) {
    // syscall tracer is disabled by default, enable() must be called with the trace output path to enable it
    m_output_file = fopen(output_path, "w");
    if (!m_output_file) {
      std::cerr << "Fail to enable syscall trace: unable to open [" << output_path << "] to write" << std::endl;
      exit(1);
    }
    m_enabled = true;
  }

  void syscall_record_begin(const char *scall_name, uint64_t scall_id) {
    if (!m_enabled)
      return;
    fprintf(m_output_file, "[%" PRIu64 "] %s (\n", scall_id, scall_name);
  }

  void syscall_record_end(uint64_t ret_code) {
    if (!m_enabled)
      return;
    fprintf(m_output_file, ") -> %" PRIi64 "\n\n", (int64_t) ret_code);
  }

  void syscall_record_param_uint64(const char *param_name, uint64_t value) {
    if (!m_enabled)
      return;
    fprintf(m_output_file, "  uint64_t %s = %" PRIu64 "\n", param_name, value);
  }

  void syscall_record_param_int64(const char *param_name, int64_t value) {
    if (!m_enabled)
      return;
    fprintf(m_output_file, "  int64_t %s = %" PRIi64 "\n", param_name, value);
  }

  void syscall_record_param_fd(const char *param_name, int64_t value) {
    if (!m_enabled)
      return;
    fprintf(m_output_file, "  fd_t %s = %" PRIi64 "\n", param_name, value);
  }

  void syscall_record_param_simple_ptr(const char *param_name, uintptr_t ptr_val, char io_direction) {
    if (!m_enabled)
      return;
    const char *type_prefix;
    if (io_direction == 'i') {
      type_prefix = "ptr_in_t";
    } else {
      assert(io_direction == 'o');
      type_prefix = "ptr_out_t";
    }
    fprintf(m_output_file, "  %s %s = 0x%016" PRIX64 "\n", type_prefix, param_name, ptr_val);
  }

  void syscall_record_param_path_name(const char *param_name, uint64_t ptr_val, const char *ptr_dat, char io_direction) {
    if (!m_enabled)
      return;
    const char *type_prefix;
    if (io_direction == 'i') {
      type_prefix = "path_in_t";
    } else {
      assert(io_direction == 'o');
      type_prefix = "path_out_t";
    }
    fprintf(
      m_output_file, "  %s %s = 0x%016" PRIX64 "|%s|\n",
      type_prefix, param_name, ptr_val, base64_encode(ptr_dat).c_str()
    );
  }

  void syscall_record_param_str(const char *param_name, uint64_t ptr_val, const char *ptr_dat, char io_direction) {
    if (!m_enabled)
      return;
    const char *type_prefix;
    if (io_direction == 'i') {
      type_prefix = "str_in_t";
    } else {
      assert(io_direction == 'o');
      type_prefix = "str_out_t";
    }
    fprintf(
      m_output_file, "  %s %s = 0x%016" PRIX64 "|%s|\n",
      type_prefix, param_name, ptr_val, base64_encode(ptr_dat).c_str()
    );
  }

private:
  bool m_enabled = false;
  FILE *m_output_file = nullptr;
};


#endif //RISCV_ISA_SIM_STRACE_H
