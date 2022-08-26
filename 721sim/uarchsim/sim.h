// See LICENSE for license details.

#ifndef _RISCV_SIM_H
#define _RISCV_SIM_H

#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <gzstream.h>
//#include "pipeline.h"
#include "mmu.h"

#define DEBUG_MMU true
#define MICRO_MMU true

enum proc_type_t {ISA_SIM,MICRO_SIM};

class htif_isasim_t;
class debug_buffer_t;

// this class encapsulates the processors and memory in a RISC-V machine.
class sim_t
{
public:
	sim_t(size_t _nprocs, size_t mem_mb, const std::vector<std::string>& htif_args, proc_type_t _proc_type);
	~sim_t();

	// run the simulation to completion
	void boot();
	int run();
	bool running();
	void stop();
	void set_debug(bool value);
	void set_histogram(bool value);
	void set_procs_debug(bool value);
	void set_procs_checker(bool value);
	bool get_procs_debug();
	bool get_procs_checker();
	htif_isasim_t* get_htif() {
		return htif.get();
	}

#ifdef RISCV_ENABLE_SIMPOINT
  void set_simpoint(bool enable, size_t interval);
#endif

	// deliver an IPI to a specific processor
	void send_ipi(reg_t who);

	// returns the number of processors in this simulator
	size_t num_cores() {
		return procs.size();
	}
	processor_t* get_core(size_t i) {
		return procs.at(i);
	}

  void init_checkpoint(std::string _checkpoint_file);
  bool create_checkpoint();
  bool restore_checkpoint(std::string restore_file);


	// read one of the system control registers
	reg_t get_scr(int which);

  void set_procs_pipe(debug_buffer_t* pipe);

  void step_till_pc(reg_t break_pc,unsigned int proc_n);

  bool run_fast(size_t n);

  proc_type_t get_proc_type(){return proc_type;}

private:
  proc_type_t proc_type;
	std::unique_ptr<htif_isasim_t> htif;
	char* mem; // main memory
	size_t memsz; // memory size in bytes
	mmu_t* debug_mmu;  // debug port into main memory
	std::vector<processor_t*> procs;

	bool step(); // Step 1 cycle.
	static const size_t INTERLEAVE = 64;
	size_t current_step;
	size_t idle_cycles;
	size_t current_proc;
	bool debug;
	bool histogram_enabled; // provide a histogram of PCs
  bool checkpointing_enabled;
  std::string checkpoint_file;

	// presents a prompt for introspection into the simulation
	void interactive();

	// functions that help implement interactive()
	void interactive_quit(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_run(const std::string& cmd, const std::vector<std::string>& args, bool noisy);
	void interactive_run_noisy(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_run_silent(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_reg(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_fregs(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_fregd(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_mem(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_str(const std::string& cmd, const std::vector<std::string>& args);
	void interactive_until(const std::string& cmd, const std::vector<std::string>& args);
	reg_t get_reg(const std::vector<std::string>& args);
	reg_t get_freg(const std::vector<std::string>& args);
	reg_t get_mem(const std::vector<std::string>& args);
	reg_t get_pc(const std::vector<std::string>& args);
	reg_t get_tohost(const std::vector<std::string>& args);

  //void create_memory_checkpoint(std::string memory_file);
  //void restore_memory_checkpoint(std::string memory_file);
  //void create_proc_checkpoint(std::string proc_file);
  //void restore_proc_checkpoint(std::string proc_file);

  //std::fstream proc_chkpt;
  //std::fstream restore_chkpt;
  ogzstream proc_chkpt;
  igzstream restore_chkpt;
  void create_memory_checkpoint(std::ostream& memory_chkpt);
  void restore_memory_checkpoint(std::istream& memory_chkpt);
  void create_register_checkpoint(std::ostream& proc_chkpt);
  void restore_proc_checkpoint(std::istream& proc_chkpt);

	friend class htif_isasim_t;
  friend class debug_buffer_t;
};

extern volatile bool ctrlc_pressed;

#endif
