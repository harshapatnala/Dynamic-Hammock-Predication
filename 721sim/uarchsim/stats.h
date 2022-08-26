#ifndef STATS_H
#define STATS_H

#include <cinttypes>
#include <cstring>
#include <map>
#include <cstdio>
#include <string>


// Statistics related variables and funcions

#define inc_counter(x)  stats->update_counter(#x,1)
#define inc_counter_str(x)  stats->update_counter(x,1)
#define dec_counter(x)  stats->update_counter(#x,-1)
#define counter(x)      stats->get_counter(#x)
#define knob(x)         stats->get_knob(#x)

// Macro has been written this way to swallow semicolon
#define DECLARE_COUNTER(stats,name,hierarchy) \
  do  {\
    stats->register_counter(#name, #hierarchy);  \
  } while(0) 

// Macro has been written this way to swallow semicolon
#define DECLARE_RATE(stats,name,hierarchy,numerator,denominator,multiplier) \
  do  {\
    stats->register_rate(#name, #hierarchy, #numerator, #denominator,multiplier);  \
  } while(0) 
  
// Macro has been written this way to swallow semicolon
#define DECLARE_PHASE_COUNTER(stats,name,hierarchy) \
  do  {\
    stats->register_phase_counter(#name, #hierarchy);  \
  } while(0) 

// Macro has been written this way to swallow semicolon
#define DECLARE_PHASE_RATE(stats,name,hierarchy,numerator,denominator,multiplier) \
  do  {\
    stats->register_phase_rate(#name, #hierarchy, #numerator, #denominator,multiplier);  \
  } while(0) 
  

// Macro has been written this way to swallow semicolon
#define DECLARE_KNOB(stats,name,value,hierarchy) \
  do  {\
    stats->register_knob(#name, #hierarchy, value);  \
  } while(0) 

 

struct ltstr
{
    bool operator()(std::string s1, std::string s2) const {
        return strcmp(s1.c_str(), s2.c_str()) < 0;
    }
};

typedef struct counter {
  uint64_t count;
  uint64_t phase_count;
  char* name;
  char* hierarchy;
  bool valid_phase_counter;   // When "true", indicates this must be dumped for each phase
} counter_t;

typedef struct rate {
  double rate;
  double phase_rate;
  double multiplier;
  char* name;
  char* hierarchy;
  char* numerator;
  char* denominator;
  bool valid_phase_rate; // When "true", indicates this must be dumped for each phase
} rate_t;

typedef struct knob {
  unsigned int value;
  char* name;
  char* hierarchy;
} knob_t;

typedef struct branch {
  size_t pc;
  size_t executed;
  size_t mispredicted;
} branch_t;

//Forward declaring classes
class pipeline_t;

class stats_t {
public:

  stats_t(pipeline_t* _proc);
  ~stats_t(){}
  void set_phase_interval(const char* name,uint64_t interval);
  void update_counter(const char* name,unsigned int inc=1);
  void update_pc_histogram(size_t pc);
  void update_br_histogram(size_t pc,bool misp);
  uint64_t get_counter(const char* name);
  unsigned int get_knob(const char* name);
  void register_counter(const char* name, const char* hierarchy);
  void register_phase_counter(const char* name, const char* hierarchy);
  void register_rate(const char* name, const char* hierarchy, const char* numerator, const char* denominator, double multiplier);
  void register_phase_rate(const char* name, const char* hierarchy, const char* numerator, const char* denominator, double multiplier);
  void register_knob(const char* name, const char* hierarchy, unsigned int value);
  void set_log_files(FILE* _stats_log, FILE* _phase_log);

  void reset_counters();
  void reset_phase_counters();
  void update_rates();
  void dump_counters();  
  void dump_phase_counters();  
  void dump_rates();  
  void dump_phase_rates();  
  void dump_knobs();  
  void dump_pc_histogram();  
  void dump_br_histogram();
  uint64_t fake_count;  

  //inline void set_histogram(bool val){histogram_enabled = val;}

private:

  std::map<std::string, counter_t*, ltstr> counter_map;
  std::map<std::string, rate_t*, ltstr> rate_map;
  //map<const char*, counter_t*, ltstr> phase_counter_map;
  std::map<std::string, knob_t*, ltstr> knob_map;
  std::map<size_t,size_t> pc_histogram;
  std::map<size_t,branch_t> br_histogram;

  uint64_t phase_id;
  uint64_t phase_interval;
  char phase_counter_name[16];
  FILE* stats_log;
  FILE* phase_log;

  pipeline_t* proc;
  //bool histogram_enabled;

  void phase_tick();
};

#endif //STATS_H
