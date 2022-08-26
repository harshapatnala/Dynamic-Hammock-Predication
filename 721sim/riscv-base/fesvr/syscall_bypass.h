#ifndef _SYSCALL_BYPASS_H
#define _SYSCALL_BYPASS_H

#include "syscall.h"
#include "htif.h"
#include "memif.h"
#include <functional>
#include <iostream>

#define CORE_SEQ_QUEUE_MAX 8

struct syscall_mem_transaction_t {
public:
  addr_t taddr;
  size_t len;
  const uint8_t *data;
  bool is_write;

  syscall_mem_transaction_t(addr_t taddr, size_t len, const void *data, bool is_write) {
    auto dat_buf = new uint8_t[len];
    memcpy(dat_buf, data, len);
    this->taddr = taddr;
    this->len = len;
    this->data = dat_buf;
    this->is_write = is_write;
  };

  ~syscall_mem_transaction_t() {
    delete[] this->data;
  }
};

struct syscall_service_sequence_t {
private:
  size_t ref_cnt;
public:
  reg_t seq_payload;
  uint32_t seq_coreid;
  uint64_t seq_respond;
  int seq_final_htif_exitcode;
  std::vector<syscall_mem_transaction_t *> seq_trans;

  explicit syscall_service_sequence_t(reg_t seq_tohost, uint32_t seq_coreid, size_t ref_cnt) :
    ref_cnt(ref_cnt), seq_payload(seq_tohost), seq_coreid(seq_coreid) {
    assert(ref_cnt != 0);
    seq_respond = 0;
    seq_final_htif_exitcode = 0;
  };

  syscall_service_sequence_t(syscall_service_sequence_t &&o) noexcept:
    ref_cnt(o.ref_cnt), seq_payload(o.seq_payload), seq_coreid(o.seq_coreid), seq_respond(o.seq_respond),
    seq_final_htif_exitcode(o.seq_final_htif_exitcode), seq_trans(std::move(o.seq_trans)) {
    o.seq_trans.clear();
    o.ref_cnt = 0;
  };

  ~syscall_service_sequence_t() {
    for (auto &t: seq_trans) {
      delete t;
    }
  }

  static void free(syscall_service_sequence_t *&seq) {
    if (seq->ref_cnt != 0) {
      --seq->ref_cnt;
    }

    if (seq->ref_cnt == 0) {
      delete seq;
    }
    seq = nullptr;
  }
};

class memif_tap_listener_t {
public:
  virtual void on_mem_read(addr_t addr, size_t len, void *bytes) = 0;

  virtual void on_mem_write(addr_t addr, size_t len, const void *bytes) = 0;
};

class memif_tap_t : public memif_t {
public:
  memif_tap_t(htif_t *htif, memif_tap_listener_t &listener);

  void read(addr_t addr, size_t len, void *bytes) override;

  void write(addr_t addr, size_t len, const void *bytes) override;

private:
  memif_tap_listener_t &m_listener;

};

class syscall_mirror_t : public syscall_t {
private:
  std::vector<
    std::queue<syscall_service_sequence_t *>
  > cores_queued_service_seq;
public:
  explicit syscall_mirror_t(htif_t *htif);

  void notify_syscall_sequence(syscall_service_sequence_t *new_seq);

  void syscall_handler_mirror(command_t cmd);

  void enable_strace(const char *output_path) override;

  void dump_std_out_err(const char *stdout_dump_path, const char *stderr_dump_path) override;

  void init_target_cwd(const char *cwd) override;
};

class syscall_main_t : public syscall_t, public memif_tap_listener_t {
private:
  memif_tap_t memif_tap;
  std::vector<syscall_mirror_t *> mirrors;
  syscall_service_sequence_t *active_sequence;

public:
  explicit syscall_main_t(htif_t *htif);

  void register_mirror(syscall_mirror_t *mirror);

  void syscall_handler_main(command_t cmd);

  void on_mem_read(addr_t addr, size_t len, void *bytes) override;

  void on_mem_write(addr_t addr, size_t len, const void *bytes) override;
};

class bypassed_syscall_device_auto_factory {
public:
  static syscall_t *make_syscall_device(htif_t *htif);
};


#endif //_SYSCALL_BYPASS_H
