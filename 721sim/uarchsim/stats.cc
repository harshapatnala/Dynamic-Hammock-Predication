#include "stats.h"
#include "pipeline.h"
#include "parameters.h"

stats_t::stats_t(pipeline_t* _proc){

  this->proc = _proc;
  fake_count = 0;

  DECLARE_COUNTER(this, cycle_count               ,proc);
  DECLARE_COUNTER(this, commit_count              ,proc);
  DECLARE_COUNTER(this, ld_vio_count              ,proc);
#if 0
  DECLARE_COUNTER(this, load_count                ,proc);
  DECLARE_COUNTER(this, store_count               ,proc);
  DECLARE_COUNTER(this, fp_count                  ,proc);
  DECLARE_COUNTER(this, branch_count              ,proc);
  DECLARE_COUNTER(this, cond_branch_count         ,proc);
  //DECLARE_COUNTER(this, uncond_branch_count       ,proc);
  DECLARE_COUNTER(this, mispredict_count          ,proc);
  //DECLARE_COUNTER(this, exception_count           ,proc);
  DECLARE_COUNTER(this, spec_inst_count           ,proc);
  //DECLARE_COUNTER(this, spec_cond_branch_count    ,proc);
  //DECLARE_COUNTER(this, spec_uncond_branch_count  ,proc);
  //DECLARE_COUNTER(this, spec_mispredict_count     ,proc);
  //DECLARE_COUNTER(this, load_stall_count          ,proc);
  //DECLARE_COUNTER(this, load_stall_miss_count     ,proc);
  //DECLARE_COUNTER(this, load_forward_count        ,proc);
  DECLARE_COUNTER(this, spec_load_count           ,proc);
  DECLARE_COUNTER(this, spec_store_count          ,proc);
  DECLARE_COUNTER(this, load_replay_count         ,proc);
  DECLARE_COUNTER(this, load_miss_count           ,proc);
  DECLARE_COUNTER(this, store_miss_count          ,proc);
  DECLARE_COUNTER(this, spec_load_miss_count      ,proc);
  DECLARE_COUNTER(this, spec_store_miss_count     ,proc);
  DECLARE_COUNTER(this, store_mhsr_miss_count     ,proc);
  DECLARE_COUNTER(this, load_mhsr_miss_count      ,proc);
  DECLARE_COUNTER(this, ld_replay_mhsr_miss_count ,proc);

  DECLARE_COUNTER(this, fetched_bundle_count      ,proc);
  DECLARE_COUNTER(this, fetched_inst_count        ,proc);
  DECLARE_COUNTER(this, btb_write_count           ,proc);
  DECLARE_COUNTER(this, bp_write_count            ,proc);
  DECLARE_COUNTER(this, ras_read_count            ,proc);
  DECLARE_COUNTER(this, ras_write_count           ,proc);
  DECLARE_COUNTER(this, ctiq_read_count           ,proc);
  DECLARE_COUNTER(this, ctiq_write_count          ,proc);
  DECLARE_COUNTER(this, dispatched_bundle_count   ,proc);
  DECLARE_COUNTER(this, dispatched_inst_count     ,proc);
  DECLARE_COUNTER(this, dispatched_load_count     ,proc);
  DECLARE_COUNTER(this, dispatched_store_count    ,proc);
  DECLARE_COUNTER(this, issued_bundle_count       ,proc);
  DECLARE_COUNTER(this, issued_inst_count         ,proc);
  DECLARE_COUNTER(this, retired_bundle_count      ,proc);
  DECLARE_COUNTER(this, retired_inst_count        ,proc);
  DECLARE_COUNTER(this, lane0_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane1_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane2_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane3_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane4_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane5_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane6_inst_executed_count ,proc);
  DECLARE_COUNTER(this, lane7_inst_executed_count ,proc);
  DECLARE_COUNTER(this, prf_read_count            ,proc);
  DECLARE_COUNTER(this, prf_write_count           ,proc);
  DECLARE_COUNTER(this, rmt_write_count           ,proc);
  DECLARE_COUNTER(this, amt_write_count           ,proc);
  DECLARE_COUNTER(this, recovery_count            ,proc);
  DECLARE_COUNTER(this, wakeup_cam_read_count     ,proc);
  DECLARE_COUNTER(this, freelist_write_count      ,proc);
#endif

  DECLARE_RATE(this, ipc_rate, proc, commit_count, cycle_count, 1.0);
#if 0
  DECLARE_RATE(this, mispredict_rate, proc, mispredict_count, cond_branch_count, 100);
  DECLARE_RATE(this, mpki_rate, proc, mispredict_count, commit_count, 1000.0);
  DECLARE_RATE(this, ld_mpki_rate, proc, load_miss_count, commit_count, 1000.0);
  //DECLARE_RATE(this, spec_mispredict_rate, proc, spec_mispredict_count, spec_cond_branch_count, 1.0);
  //DECLARE_RATE(this, spec_mpki_rate, proc, mispredict_count, commit_count, 1000.0);
#endif

  DECLARE_PHASE_COUNTER(this, cycle_count               ,proc);
  DECLARE_PHASE_COUNTER(this, commit_count              ,proc);

#if 0
  if(verbose_phase_counters){
    DECLARE_PHASE_COUNTER(this, load_count                ,proc);
    DECLARE_PHASE_COUNTER(this, store_count               ,proc);
    DECLARE_PHASE_COUNTER(this, fp_count                  ,proc);
    DECLARE_PHASE_COUNTER(this, branch_count              ,proc);
    DECLARE_PHASE_COUNTER(this, cond_branch_count         ,proc);
    DECLARE_PHASE_COUNTER(this, mispredict_count          ,proc);
    DECLARE_PHASE_COUNTER(this, spec_inst_count           ,proc);
    DECLARE_PHASE_COUNTER(this, spec_load_count           ,proc);
    DECLARE_PHASE_COUNTER(this, spec_store_count          ,proc);
    DECLARE_PHASE_COUNTER(this, load_replay_count         ,proc);
    DECLARE_PHASE_COUNTER(this, load_miss_count           ,proc);
    DECLARE_PHASE_COUNTER(this, store_miss_count          ,proc);
    DECLARE_PHASE_COUNTER(this, spec_load_miss_count      ,proc);
    DECLARE_PHASE_COUNTER(this, spec_store_miss_count     ,proc);
    DECLARE_PHASE_COUNTER(this, store_mhsr_miss_count     ,proc);
    DECLARE_PHASE_COUNTER(this, load_mhsr_miss_count      ,proc);
    DECLARE_PHASE_COUNTER(this, ld_replay_mhsr_miss_count ,proc);
  
    DECLARE_PHASE_COUNTER(this, fetched_bundle_count      ,proc);
    DECLARE_PHASE_COUNTER(this, fetched_inst_count        ,proc);
    DECLARE_PHASE_COUNTER(this, btb_write_count           ,proc);
    DECLARE_PHASE_COUNTER(this, bp_write_count            ,proc);
    DECLARE_PHASE_COUNTER(this, ras_read_count            ,proc);
    DECLARE_PHASE_COUNTER(this, ras_write_count           ,proc);
    DECLARE_PHASE_COUNTER(this, ctiq_read_count           ,proc);
    DECLARE_PHASE_COUNTER(this, ctiq_write_count          ,proc);
    DECLARE_PHASE_COUNTER(this, dispatched_bundle_count   ,proc);
    DECLARE_PHASE_COUNTER(this, dispatched_inst_count     ,proc);
    DECLARE_PHASE_COUNTER(this, dispatched_load_count     ,proc);
    DECLARE_PHASE_COUNTER(this, dispatched_store_count    ,proc);
    DECLARE_PHASE_COUNTER(this, issued_bundle_count       ,proc);
    DECLARE_PHASE_COUNTER(this, issued_inst_count         ,proc);
    DECLARE_PHASE_COUNTER(this, retired_bundle_count      ,proc);
    DECLARE_PHASE_COUNTER(this, retired_inst_count        ,proc);
    DECLARE_PHASE_COUNTER(this, lane0_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane1_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane2_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane3_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane4_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane5_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane6_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, lane7_inst_executed_count ,proc);
    DECLARE_PHASE_COUNTER(this, prf_read_count            ,proc);
    DECLARE_PHASE_COUNTER(this, prf_write_count           ,proc);
    DECLARE_PHASE_COUNTER(this, rmt_write_count           ,proc);
    DECLARE_PHASE_COUNTER(this, amt_write_count           ,proc);
    DECLARE_PHASE_COUNTER(this, recovery_count            ,proc);
    DECLARE_PHASE_COUNTER(this, wakeup_cam_read_count     ,proc);
    DECLARE_PHASE_COUNTER(this, freelist_write_count      ,proc);
  }
#endif

  DECLARE_PHASE_RATE(this, ipc_rate, proc, commit_count, cycle_count, 1.0);
#if 0
  DECLARE_PHASE_RATE(this, mispredict_rate, proc, mispredict_count, cond_branch_count, 1.0);
  DECLARE_PHASE_RATE(this, mpki_rate, proc, mispredict_count, commit_count, 1000.0);
#endif

  reset_counters();
  reset_phase_counters();
  set_phase_interval("commit_count",10000);
  phase_id = 0;

}

void stats_t::set_log_files(FILE* _stats_log,FILE* _phase_log){
  this->stats_log = _stats_log;
  this->phase_log = _phase_log;
}

void stats_t::set_phase_interval(const char* name,uint64_t interval)
{
  std::strcpy(phase_counter_name,name);
  phase_interval = interval;
  ifprintf(logging_on,stderr,"Setting phase interval to %s = %lu\n",phase_counter_name,interval);
}

void stats_t::reset_counters(){
  std::map<std::string, counter_t*, ltstr>::iterator ctr_iter;
  for(ctr_iter = counter_map.begin();ctr_iter != counter_map.end(); ctr_iter++){
    ctr_iter->second->count = 0;
  }
}

void stats_t::reset_phase_counters(){
  std::map<std::string, counter_t*, ltstr>::iterator ctr_iter;
  for(ctr_iter = counter_map.begin();ctr_iter != counter_map.end(); ctr_iter++){
    ctr_iter->second->phase_count = 0;
  }
}

void stats_t::register_counter(const char* name, const char* hierarchy){

  counter_t* c    = new counter_t;
  c->count        = 0;
  c->phase_count  = 0;
  c->name         = new char[strlen(name)+1];
  c->hierarchy    = new char[strlen(hierarchy)+1];
  c->valid_phase_counter    = false;
  strcpy(c->name,name);
  strcpy(c->hierarchy,hierarchy);
  counter_map[name] = c;
  ifprintf(logging_on,stderr,"Counter name %s %s\n",name,hierarchy);
}

void stats_t::register_phase_counter(const char* name, const char* hierarchy){
  // If the counter has been declared, mark it as a phase counter
  if(counter_map.find(name) != counter_map.end()){
    counter_map[name]->valid_phase_counter = true;
  } 
  // If it does not exist, declare it and mark it as a phase counter
  else {
    counter_t* c    = new counter_t;
    c->name         = new char[strlen(name)+1];
    c->hierarchy    = new char[strlen(hierarchy)+1];
    c->valid_phase_counter  = true;
    strcpy(c->name,name);
    strcpy(c->hierarchy,hierarchy);
    counter_map[name] = c;
  }
}

void stats_t::register_rate(const char* name, const char* hierarchy, const char* numerator, const char* denominator, double multiplier){
  rate_t* r       = new rate_t;
  r->name         = new char[strlen(name)+1];
  r->hierarchy    = new char[strlen(hierarchy)+1];
  r->numerator    = new char[strlen(numerator)+1];
  r->denominator  = new char[strlen(denominator)+1];
  r->valid_phase_rate    = false;
  strcpy(r->name,name);
  strcpy(r->hierarchy,hierarchy);
  strcpy(r->numerator,numerator);
  strcpy(r->denominator,denominator);
  r->multiplier = multiplier;
  rate_map[name] = r;
}

void stats_t::register_phase_rate(const char* name, const char* hierarchy, const char* numerator, const char* denominator, double multiplier){
  // If the counter has been declared, mark it as a phase counter
  if(rate_map.find(name) != rate_map.end()){
    rate_map[name]->valid_phase_rate = true;
  }
  // If it does not exist, declare it and mark it as a phase counter
  else { 
    rate_t* r       = new rate_t;
    r->name         = new char[strlen(name)+1];
    r->hierarchy    = new char[strlen(hierarchy)+1];
    r->numerator    = new char[strlen(numerator)+1];
    r->denominator  = new char[strlen(denominator)+1];
    r->valid_phase_rate  = true;
    strcpy(r->name,name);
    strcpy(r->hierarchy,hierarchy);
    strcpy(r->numerator,numerator);
    strcpy(r->denominator,denominator);
    r->multiplier = multiplier;
    rate_map[name] = r;
  }
}


void stats_t::register_knob(const char* name, const char* hierarchy, unsigned int value){
  knob_t* k       = new knob_t;
  k->name         = new char[strlen(name)+1];
  k->hierarchy    = new char[strlen(hierarchy)+1];
  k->value        = value;
  strcpy(k->name,name);
  strcpy(k->hierarchy,hierarchy);
  knob_map[name] = k;
}


void stats_t::update_counter(const char* name,unsigned int inc){
  // If the counter has been declared and initialized
  if(counter_map.find(name) != counter_map.end()){
    counter_map[name]->count++;
    counter_map[name]->phase_count++;
  }
  // Tick the phase check mechanism if updating the 
  // counter on which phases are based on. Normally this
  // would be commit_count or cycle_count.
  if(!std::strcmp(name, phase_counter_name)){
    phase_tick();
  }
}

uint64_t stats_t::get_counter(const char* name){
  return counter_map[name]->count;
}

unsigned int stats_t::get_knob(const char* name){
  return knob_map[name]->value;
}

void stats_t::phase_tick(){
  if(counter_map[phase_counter_name]->phase_count >= phase_interval){
    phase_id++;
    update_rates();
    dump_phase_counters();
    dump_phase_rates();
    //dump_counters();
    //dump_rates();
    reset_phase_counters();
    fflush(0);
  }
}

void stats_t::update_rates(){
  std::map<std::string, rate_t*, ltstr>::iterator rate_iter;
  for(rate_iter = rate_map.begin();rate_iter != rate_map.end(); rate_iter++){
    if(counter_map[rate_iter->second->denominator]->count == 0){
      rate_iter->second->rate = (double)0.0;
    } else {
      rate_iter->second->rate = rate_iter->second->multiplier*
                                double(counter_map[rate_iter->second->numerator]->count)/
                                double(counter_map[rate_iter->second->denominator]->count);
    }

    if(counter_map[rate_iter->second->denominator]->phase_count == 0){
      rate_iter->second->phase_rate = (double)0.0;
    } else {
      rate_iter->second->phase_rate = rate_iter->second->multiplier*
                                      double(counter_map[rate_iter->second->numerator]->phase_count)/
                                      double(counter_map[rate_iter->second->denominator]->phase_count);
    }
  }
}

void stats_t::dump_counters(){
  fprintf(stats_log,"[stats]\n");
  std::map<std::string, counter_t*, ltstr>::iterator ctr_iter;
  for(ctr_iter = counter_map.begin();ctr_iter != counter_map.end(); ctr_iter++){
    fprintf(stats_log,"%s : %" PRIu64 "\n",ctr_iter->second->name, ctr_iter->second->count);
  }
  fprintf(stats_log, "Fake Retired Instruction Count : %llu\n", fake_count);
}

void stats_t::dump_rates(){
  fprintf(stats_log,"[rates]\n");
  std::map<std::string, rate_t*, ltstr>::iterator rate_iter;
  for(rate_iter = rate_map.begin();rate_iter != rate_map.end(); rate_iter++){
    fprintf(stats_log,"%s : %2.2f\n",rate_iter->second->name, rate_iter->second->rate);
  }
}

void stats_t::dump_phase_counters(){
  fprintf(phase_log,"-------- Phase Counters Phase ID %" PRIu64 "--------\n",phase_id);
  std::map<std::string, counter_t*, ltstr>::iterator ctr_iter;
  for(ctr_iter = counter_map.begin();ctr_iter != counter_map.end(); ctr_iter++){
    if(ctr_iter->second->valid_phase_counter)
      fprintf(phase_log,"%s : %" PRIu64 "\n",ctr_iter->second->name, ctr_iter->second->phase_count);
  }
}

void stats_t::dump_phase_rates(){
  fprintf(phase_log,"-------- Phase Rates Phase ID %" PRIu64 "--------\n",phase_id);
  std::map<std::string, rate_t*, ltstr>::iterator rate_iter;
  for(rate_iter = rate_map.begin();rate_iter != rate_map.end(); rate_iter++){
    if(rate_iter->second->valid_phase_rate)
      fprintf(phase_log,"%s : %2.2f\n",rate_iter->second->name, rate_iter->second->phase_rate);
  }
}

void stats_t::dump_knobs(){
  fprintf(stats_log,"[knobs]\n");
  std::map<std::string, knob_t*, ltstr>::iterator knb_iter;
  for(knb_iter = knob_map.begin();knb_iter != knob_map.end(); knb_iter++){
    fprintf(stats_log,"%s : %u\n",knb_iter->second->name, knb_iter->second->value);
  }
}


void stats_t::update_pc_histogram(size_t pc){
  // If the counter has been declared and initialized
  pc_histogram[pc >> 2]++;
}

void stats_t::update_br_histogram(size_t pc,bool misp){
  // If the counter has been declared and initialized
  br_histogram[pc >> 2].executed++;
  if(misp)
    br_histogram[pc >> 2].mispredicted ++;
}

void stats_t::dump_pc_histogram(){
  if (proc->get_histogram())
  {
    fprintf(stderr, "PC Histogram size:%lu\n", pc_histogram.size());
    fprintf(stats_log, "-------PC Histogram-------\n");
    for(auto iterator = pc_histogram.begin(); iterator != pc_histogram.end(); ++iterator) {
      fprintf(stats_log, "%0lx %lu\n", (iterator->first << 2), iterator->second);
    }
  }
}

void stats_t::dump_br_histogram(){
  if (proc->get_histogram())
  {
    fprintf(stderr, "BR Histogram size:%lu\n", br_histogram.size());
    fprintf(stats_log, "-------BR Histogram-------\n");
    for(auto iterator = br_histogram.begin(); iterator != br_histogram.end(); ++iterator) {
      fprintf(stats_log, "%0lx %lu %lu\n", (iterator->first << 2), iterator->second.executed, iterator->second.mispredicted);
    }
  }
}
