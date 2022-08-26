#include "syscall_bypass.h"

static syscall_main_t *upstream = nullptr;

syscall_t *bypassed_syscall_device_auto_factory::make_syscall_device(htif_t *htif) {
  if (upstream == nullptr) {
    upstream = new syscall_main_t(htif);
    return upstream;
  } else {
    auto mirrored = new syscall_mirror_t(htif);
    upstream->register_mirror(mirrored);
    return mirrored;
  }
}

memif_tap_t::memif_tap_t(htif_t *htif, memif_tap_listener_t &listener) : memif_t(htif), m_listener(listener) {}

void memif_tap_t::read(addr_t addr, size_t len, void *bytes) {
  memif_t::read(addr, len, bytes);
  m_listener.on_mem_read(addr, len, bytes);
}

void memif_tap_t::write(addr_t addr, size_t len, const void *bytes) {
  memif_t::write(addr, len, bytes);
  m_listener.on_mem_write(addr, len, bytes);
}

syscall_mirror_t::syscall_mirror_t(htif_t *htif) : syscall_t(htif) {
  cores_queued_service_seq.resize(CORE_SEQ_QUEUE_MAX);
  register_command(
    0,
    std::bind(
      &syscall_mirror_t::syscall_handler_mirror,
      this,
      std::placeholders::_1
    ),
    "syscall_handler_mirror"
  );
}

void syscall_mirror_t::notify_syscall_sequence(syscall_service_sequence_t *new_seq) {
  cores_queued_service_seq[new_seq->seq_coreid].push(new_seq);
}

void syscall_mirror_t::syscall_handler_mirror(command_t cmd) {
  // perform action according to the seq in the queue for this cpu core
  auto coreid = cmd.get_coreid();
  assert(!cores_queued_service_seq[coreid].empty());
  auto service_seq = cores_queued_service_seq[coreid].front();
  cores_queued_service_seq[coreid].pop();
  assert(service_seq->seq_coreid == coreid);

  if (service_seq->seq_payload != cmd.payload()) {
    throw std::runtime_error("Mirrored syscall fatal: cmd.payload != expected_payload");
  }

  // replay the transaction in this syscall service sequence
  for (auto &t: service_seq->seq_trans) {
    if (t->is_write) {
      memif->write(t->taddr, t->len, t->data);
    } else {
      std::vector<uint8_t> read_buf(t->len);
      memif->read(t->taddr, t->len, &read_buf[0]);

      if (memcmp(t->data, &read_buf[0], t->len) != 0) {
        throw std::runtime_error("Mirrored syscall fatal: data read != expected");
      }
    }
  }

  htif->exitcode = service_seq->seq_final_htif_exitcode;

  cmd.respond(service_seq->seq_respond);
  syscall_service_sequence_t::free(service_seq);
}

void syscall_mirror_t::enable_strace(const char *output_path) {}

void syscall_mirror_t::dump_std_out_err(const char *stdout_dump_path, const char *stderr_dump_path) {}

void syscall_mirror_t::init_target_cwd(const char *cwd) {}

syscall_main_t::syscall_main_t(htif_t *htif) : syscall_t(htif), memif_tap(htif, *this) {
  this->memif = &memif_tap;
  register_command(
    0,
    std::bind(
      &syscall_main_t::syscall_handler_main,
      this,
      std::placeholders::_1
    ),
    "syscall_handler_main"
  );
  active_sequence = nullptr;
}

void syscall_main_t::register_mirror(syscall_mirror_t *mirror) {
  mirrors.push_back(mirror);
}

void syscall_main_t::syscall_handler_main(command_t cmd) {
  auto recording_sequence = !mirrors.empty();

  if (recording_sequence) {
    assert(active_sequence == nullptr);
    active_sequence = new syscall_service_sequence_t(cmd.payload(), cmd.get_coreid(), mirrors.size() + 1);
  }

  handle_syscall(cmd);

  if (recording_sequence) {
    active_sequence->seq_respond = 1;
    active_sequence->seq_final_htif_exitcode = htif->exitcode;
    for (auto &m : mirrors) {
      // broadcast the recorded sequence to all mirrors
      m->notify_syscall_sequence(active_sequence);
    }
    syscall_service_sequence_t::free(active_sequence);
    assert(active_sequence == nullptr);
  }
}

void syscall_main_t::on_mem_read(addr_t addr, size_t len, void *bytes) {
  if (active_sequence) {
    auto trans = new syscall_mem_transaction_t(addr, len, bytes, false);
    active_sequence->seq_trans.push_back(trans);
  }
}

void syscall_main_t::on_mem_write(addr_t addr, size_t len, const void *bytes) {
  if (active_sequence) {
    auto trans = new syscall_mem_transaction_t(addr, len, bytes, true);
    active_sequence->seq_trans.push_back(trans);
  }
}
