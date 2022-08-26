#include <cstdio>
#include <ctime>

#include <cinttypes>
#include <unistd.h>
#include <fcntl.h>

#define CHECKPOINT_VERSION 2

#define DEBUG_CHECKPOINTS if (knobs::debug_checkpoints) printf

float weights[15];    // weights of checkpoints

extern FILE *out;

/////////////////////////////////////////////////////////////////////////////

typedef struct {
    uint32_t checkpoint_version;
    uint32_t num_checkpoints;
    uint64_t checkpoint_length;
    uint64_t text_base;
    uint64_t text_size;
    uint64_t data_base;
    uint64_t data_size;
    uint64_t bss_base;
    uint64_t bss_size;
    uint64_t tdata_base;
    uint64_t tdata_size;
    uint64_t tbss_base;
    uint64_t tbss_size;
    char     bmk_name[128];
    float    weights[15];
} checkpoint_filedata;

/////////////////////////////////////////////////////////////////////////////

//void pipeline::create_checkpoint(const char *filename, uint64_t length)
//{
//    // the checkpoint file consists of memory state and register values for the
//    // beginning of each checkpoint and memory / register diffs for each
//    // system call within each checkpoint region.  there are num checkpoint
//    // regions all from the same binary within the checkpoint file.
//    //
//    // the checkpoint file is laid out as follows:
//    //   1.  checkpoint_filedata structure
//    //   2.  array of file offsets to each checkpoint
//    //   3.  each checkpoint
//    //       4a.  number dynamic instructions from beginning to checkpoint
//    //       4b.  pc
//    //       4c.  register values
//    //       4d.  diff in memory table from previous checkpoint beginning
//    //       4e.  syscall results for each syscall in the simulation region 
//
//    bool done, overlap;
//    uint64_t *begin_points, temp;
//    uint64_t num_insts;
//    checkpoint_filedata filedata;
//    unsigned int tries, i, j, checkpoint_num;
//    FILE *fp_job;
//    FILE *fp_loader;
//    FILE *fp_about;
//    FILE *fp_checkpoint;
//    Memory *beginning_last_checkpoint;
//    char temp_filename[80];
//    char job[1024];
//
//    // open about file for writing
//    sprintf(temp_filename, "%s.about", filename);
//    fprintf(out, "opening file \"%s\" for writing\n", temp_filename);
//    fp_about = fopen(temp_filename, "w");
//
//    // write about file
//    fprintf(fp_about, "checkpoint name:    %s\n", filename);
//    fprintf(fp_about, "checkpoint version: %u\n", CHECKPOINT_VERSION);
//    fprintf(fp_about, "checkpoint bmk:     %s", bmk_name);
//    fprintf(fp_about, "checkpoint job:     %s", job);
//    fprintf(fp_about, "checkpoint type:    ");
//    fprintf(fp_about, "exact region\n");
//    fprintf(fp_about, "begin instruction:  %li\n", dynamic_begin);
//    fprintf(fp_about, "sample length:      %li\n\n", length);
//    fflush(fp_about);
//
//    // open loader datafile for writing
//    sprintf(temp_filename, "%s", filename);
//    fprintf(out, "opening file \"%s\" for writing\n", temp_filename);
//    fp_loader = fopen(temp_filename, "w");
//
//    // write loader datafile
//    fprintf(out, "writing loader data\n");
//    filedata.checkpoint_version = CHECKPOINT_VERSION;
//    filedata.num_checkpoints   = num;
//    filedata.checkpoint_length = length;
//    filedata.text_base         = text_base;
//    filedata.text_size         = text_size;
//    filedata.data_base         = data_base;
//    filedata.data_size         = data_size;
//    filedata.bss_base          = bss_base;
//    filedata.bss_size          = bss_size;
//    filedata.tdata_base        = tdata_base;
//    filedata.tdata_size        = tdata_size;
//    filedata.tbss_base         = tbss_base;
//    filedata.tbss_size         = tbss_size;
//    memset(filedata.bmk_name, '\0', 128);
//    strcpy(filedata.bmk_name, bmk_name);
//    fwrite(&filedata, sizeof(filedata), 1, fp_loader);
//
//    // close loader datafile
//    fclose(fp_loader);
//
//
//    save_current_system_state_to_checkpoint(fp_checkpoint, fp_about);
//    
//    // write memory diff against previous memory clone & clone memory here
//    memory->write_diffs_to_file(fp_checkpoint, beginning_last_checkpoint);
//    beginning_last_checkpoint->clone(memory);
//    
//    // skip to the end of the checkpoint
//    thread_iter = thread_vector.begin();
//    uint32_t num_syscalls = 0;
//    uint32_t num_pthread_calls = 0;
//
//    // done writing checkpoint here /////////////////////////////
//    fprintf(fp_about, "total traps: %" PRIu32 "\n", num_syscalls);
//    fprintf(fp_about, "total pthread calls: %" PRIu32 "\n", num_pthread_calls);
//    fflush(fp_about);
//
//    // write the system state
//    fwrite(&system.brk_point, sizeof(system.brk_point), 1, fp);
//    fwrite(&system.mmap_end, sizeof(system.mmap_end), 1, fp);
//    fwrite(&system.next_fd_translation, sizeof(system.next_fd_translation), 1, fp);
//    fwrite(&system.next_stack_base, sizeof(system.next_stack_base), 1, fp);
//    fwrite(&system.threads_spawned, sizeof(system.threads_spawned), 1, fp);
//    fwrite(&system.next_key, sizeof(system.next_key), 1, fp);
//
//
//    // flush the about file
//    if (about) 
//        fflush(about);
//
//    // print out size of file and checkpoint
//    fprintf(out, "%li bytes.\n", ftell(fp_checkpoint));
//    fflush(stdout);
//
//    // close checkpoint file
//    fclose(fp_checkpoint);
//
//    // close about file
//    fclose(fp_about);
//}

/////////////////////////////////////////////////////////////////////////////

//bool 
//Process::load_checkpoint(const char *filename, unsigned int num)
//{
//    // to load a checkpoint within a checkpoint file, we load the filedata
//    // struct, every memory diff-set (for each checkpoint beginning), the
//    // register file, the pc, the dynamic number of insts, and set the fp
//    // to the beginning of the system call results
//
//    uint64_t checkpoint_length;
//    checkpoint_filedata filedata;
//    uint32_t i, num_checkpoints;
//    vector<Thread*>::iterator thread_iter;
//    uint32_t num_threads;
//    char temp_filename[80];
//    Thread *T;
//    char thread_name[80];
//
//    // set flag for system call handler
//    using_checkpoint = true;
//
//    // close checkpoint file if already open
//    if (checkpoint_fp)
//        fclose(checkpoint_fp);
//
//    // mark the binary as not yet complete
//    program_complete = false;
//
//    // blank memory
//    memory->blank_memory();
//
//    // open loader datafile
//    sprintf(temp_filename, "%s", filename);
//    checkpoint_fp = fopen(temp_filename, "r");
//    if (!checkpoint_fp)
//        fatal("unable to open checkpoint filename \"%s\"", temp_filename);
//
//    // load filedata struct
//    fread(&filedata, sizeof(filedata), 1, checkpoint_fp);
//    if (filedata.checkpoint_version != CHECKPOINT_VERSION)
//        fatal("cannot read this version of checkpoint. (version: %d)", filedata.checkpoint_version);
//    num_checkpoints   = filedata.num_checkpoints;
//    checkpoint_length = filedata.checkpoint_length;
//    text_base         = filedata.text_base;
//    text_size         = filedata.text_size;
//    data_base         = filedata.data_base;
//    data_size         = filedata.data_size;
//    bss_base          = filedata.bss_base;
//    bss_size          = filedata.bss_size;
//    tdata_base        = filedata.tdata_base;
//    tdata_size        = filedata.tdata_size;
//    tbss_base         = filedata.tbss_base;
//    tbss_size         = filedata.tbss_size;
//
//    //// save the binary name
//    //if (!this->binary_name) {
//    //    this->binary_name = new char[strlen(filedata.bmk_name)+1];
//    //    strcpy(this->binary_name, filedata.bmk_name);
//    //}
//
//    // load checkpoint num, by applying every checkpoint prior to/including num
//    for (i = 0; i < num; i++) {
//        program_complete = false;
//        thread_vector_changed = true;
//
//        // close previous checkpoint filepointer
//        fclose(checkpoint_fp);
//
//        // open checkpoint datafile
//        sprintf(temp_filename, "%s.%04i", filename, i+1);
//        checkpoint_fp = fopen(temp_filename, "r");
//        if (!checkpoint_fp)
//            fatal("unable to open checkpoint filename \"%s\"", temp_filename);
//
//        // read number dynamic instructions from beginning of program
//        fread(&popped_inst_count, sizeof(popped_inst_count), 1, checkpoint_fp);
//        if ((i+1) == num)
//            fprintf(out, "applied checkpoint %i at %li instructions\n", 
//                   i+1, 
//                   popped_inst_count);
//        popped_inst_count = 0;
//        
//        // read the number of threads from the file
//        fread(&num_threads, sizeof(num_threads), 1, checkpoint_fp);
//
//        // add threads to the system if necessary
//        while (thread_vector.size() < num_threads) {
//            sprintf(thread_name, "%s.thread_%lu", process_name,
//                    thread_vector.size());
//            T      = new Thread(thread_name, this);
//            T->tid = MIPSARCH->threads_spawned++;
//            thread_vector.push_back(T);
//            ++(*stat_num_threads);
//        }
//
//        // now read each thread
//        for (thread_iter = thread_vector.begin();
//             thread_iter != thread_vector.end();
//             thread_iter++) {
//            T = *thread_iter;
//
//            // reset thread popped inst count
//            T->popped_inst_count       = 0;
//            T->inst_queue_head         = 0;
//            T->inst_queue_tail         = 0;
//            T->inst_queue_size         = 0;
//            T->next_inst_index         = 0;
//            T->inst_queue_stalled_trap = false;
//            T->set_delay_slot          = false;
//            T->wrong_path              = false;
//
//            // read the thread block and complete data
//            fread(&T->blocked, sizeof(T->blocked), 1, checkpoint_fp);
//            fread(&T->block_reason, 
//                   sizeof(T->block_reason), 
//                   1, 
//                   checkpoint_fp);
//            fread(&T->complete, sizeof(T->complete), 1, checkpoint_fp);
//
//            // read the stack data
//            fread(&T->stack_base, sizeof(T->stack_base), 1, checkpoint_fp);
//            fread(&T->stack_min, sizeof(T->stack_min), 1, checkpoint_fp);
//            
//            // read architectural state 
//            fread(&T->context.pc, sizeof(T->context.pc), 1, checkpoint_fp);
//            fread(T->context.regs, sizeof(T->context.regs[0]), NUM_REGS, checkpoint_fp);
//
//            // read the retired instruction count
//            fread(&T->num_insts, sizeof(T->num_insts), 1, checkpoint_fp);
//            
//            // read the pthread id
//            fread(&T->pthread_tid, sizeof(T->pthread_tid), 1, checkpoint_fp);
//
//            // finish initializing the context
//            T->init_context();
//        }
//
//        // read the current system state, but only apply it if it's the
//        // real checkpoint.
//        load_current_system_state_from_checkpoint(checkpoint_fp, 
//                                                  i == (num-1));
//        
//        // apply memory diff
//        memory->read_diffs_from_file(checkpoint_fp);
//    }
//
//    // calculate end of checkpoint region (program_complete is set after)
//    checkpoint_region_end = popped_inst_count + checkpoint_length;
//
//    // set status to successful
//    program_status = -1;
//
//    // write memory file to disk
//    sprintf(temp_filename, "%s.memory", process_name);
//    memory->push_table_to_disk(temp_filename);
//
//    // return true on successful load
//    return true;
//}

///////////////////////////////////////////////////////////////////////////////
//
//void Process::save_current_system_state_to_checkpoint(FILE *fp, FILE *about)
//{
//    map<int64_t, MIPSSystemState_FD>::iterator fd_iter;
//    map<uint64_t, MIPSSystemState_PThread>::iterator pthread_iter;
//    map<uint64_t, MIPSSystemState_Mutex>::iterator mutex_iter;
//    map<uint64_t, MIPSSystemState_Key>::iterator key_iter;
//    map<uint64_t, MIPSSystemState_Barrier>::iterator barrier_iter;
//    map<uint64_t, MIPSSystemState_Cond>::iterator cond_iter;
//    uint64_t count, inner_count;
//
//    // write the mips system state
//    fwrite(&system.brk_point, sizeof(system.brk_point), 1, fp);
//    fwrite(&system.mmap_end, sizeof(system.mmap_end), 1, fp);
//    fwrite(&system.next_fd_translation, sizeof(system.next_fd_translation), 1, fp);
//    fwrite(&system.next_stack_base, sizeof(system.next_stack_base), 1, fp);
//    fwrite(&system.threads_spawned, sizeof(system.threads_spawned), 1, fp);
//    fwrite(&system.next_key, sizeof(system.next_key), 1, fp);
//
//    // fd translation ///////////////////////////////////////////////////
//    count = system.fd_translation.size();
//    fwrite(&count, sizeof(count), 1, fp);
//    if (about)
//        fprintf(about, "translated file descriptors: %" PRIu64"\n", count);
//    for (fd_iter = system.fd_translation.begin();
//         fd_iter != system.fd_translation.end();
//         fd_iter++) {
//        fwrite(&fd_iter->first, sizeof(fd_iter->first), 1, fp);
//        fwrite(fd_iter->second.pathname,
//               sizeof(char),
//               SYSCALL_BUFFER_SIZE,
//               fp);
//        fwrite(&fd_iter->second.local_flags,
//               sizeof(fd_iter->second.local_flags),
//               1,
//               fp);
//        fwrite(&fd_iter->second.mode,
//               sizeof(fd_iter->second.mode),
//               1,
//               fp);
//        fwrite(&fd_iter->second.system_fd,
//               sizeof(fd_iter->second.system_fd),
//               1,
//               fp);
//        fwrite(&fd_iter->second.system_fd_number,
//               sizeof(fd_iter->second.system_fd_number),
//               1,
//               fp);
//
//        // get the offset currently pointed to by the fd
//        int64_t offset = -1;
//
//        if (!fd_iter->second.system_fd) {
//            // don't move the pointer, just get the offset
//            offset = lseek(fd_iter->second.fd_simulator, 0, SEEK_CUR);
//        
//            // write a lengthy about file entry on this open file descriptor
//            if (about) {
//                fprintf(about, "  file needed: %s\n", 
//                        fd_iter->second.pathname);
//                fprintf(about, "  flags: %" PRIu64, fd_iter->second.local_flags);
//
//                if (fd_iter->second.local_flags & O_RDONLY)
//                    fprintf(about, " O_RDONLY");
//                if (fd_iter->second.local_flags & O_WRONLY)
//                    fprintf(about, " O_WRONLY");
//                if (fd_iter->second.local_flags & O_RDWR)
//                    fprintf(about, " O_RDWR");
//                if (fd_iter->second.local_flags & O_NONBLOCK)
//                    fprintf(about, " O_NONBLOCK");
//                if (fd_iter->second.local_flags & O_APPEND)
//                    fprintf(about, " O_APPEND");
//                if (fd_iter->second.local_flags & O_CREAT)
//                    fprintf(about, " O_CREAT");
//                if (fd_iter->second.local_flags & O_TRUNC)
//                    fprintf(about, " O_TRUNC");
//                if (fd_iter->second.local_flags & O_EXCL)
//                    fprintf(about, " O_EXCL");
//                if (fd_iter->second.local_flags & O_NOCTTY)
//                    fprintf(about, " O_NOCTTY");
//                if (fd_iter->second.local_flags & O_SYNC)
//                    fprintf(about, " O_SYNC");
//                if (fd_iter->second.local_flags & O_DSYNC)
//                    fprintf(about, " O_DSYNC");
//                if (fd_iter->second.local_flags & O_RSYNC)
//                    fprintf(about, " O_RSYNC");
//                fprintf(about, "\n");
//
//                fprintf(about, "  offset: %" PRIi64"\n", offset);
//            }
//        }
//        fwrite(&offset, sizeof(offset), 1, fp);
//    }
//
//
//    // pthreads /////////////////////////////////////////////////////////
//
//    // writing the pthread stuff can get dicey.  complex ...
//
//    // system.pthreads //////////////////////////////////////////////
//    count = system.pthreads.size();
//    fwrite(&count, sizeof(count), 1, fp);
//    if (about) 
//        fprintf(about, "system pthread objects: %" PRIu64"\n", count);
//    for (pthread_iter = system.pthreads.begin();
//         pthread_iter != system.pthreads.end();
//         pthread_iter++) {
//        int64_t join = -1;
//        if (pthread_iter->second.blocked_join_thread != NULL)
//            join = pthread_iter->second.blocked_join_thread->pthread_tid;
//
//        fwrite(&pthread_iter->first, sizeof(pthread_iter->first), 1, fp);
//        fwrite(&pthread_iter->second.complete,
//               sizeof(pthread_iter->second.complete),
//               1,
//               fp);
//        fwrite(&pthread_iter->second.retval,
//               sizeof(pthread_iter->second.retval),
//               1,
//               fp);
//        fwrite(&join, sizeof(join), 1, fp);
//        fwrite(&pthread_iter->second.join_retval_ptr,
//               sizeof(pthread_iter->second.join_retval_ptr),
//               1,
//               fp);
//        fwrite(&pthread_iter->second.attr,
//               sizeof(pthread_iter->second.attr),
//               1,
//               fp);
//    }
//
//    // system.mutexes ///////////////////////////////////////////////
//    count = system.mutexes.size();
//    fwrite(&count, sizeof(count), 1, fp);
//    if (about) 
//        fprintf(about, "system mutex objects: %" PRIu64"\n", count);
//    for (mutex_iter = system.mutexes.begin();
//         mutex_iter != system.mutexes.end();
//         mutex_iter++) {
//        int64_t owner = -1;
//        int64_t tid;
//        
//        if (mutex_iter->second.owner != NULL)
//            owner = mutex_iter->second.owner->pthread_tid;
//
//        fwrite(&mutex_iter->first, sizeof(mutex_iter->first), 1, fp);
//        fwrite(&owner, sizeof(owner), 1, fp);
//
//        uint64_t n;
//        inner_count = mutex_iter->second.blocked_threads.size();
//        fwrite(&inner_count, sizeof(inner_count), 1, fp);
//        for (n = 0; n < inner_count; n++) {
//            tid = mutex_iter->second.blocked_threads[n]->pthread_tid;
//            fwrite(&tid, sizeof(tid), 1, fp);
//        }
//    }
//    
//    // system.keys //////////////////////////////////////////////////
//    count = system.keys.size();
//    fwrite(&count, sizeof(count), 1, fp);
//    if (about) 
//        fprintf(about, "system key objects: %" PRIu64"\n", count);
//    for (key_iter = system.keys.begin();
//         key_iter != system.keys.end();
//         key_iter++) {
//        map<uint64_t, uint64_t>::iterator thr_val_iter;
//
//        fwrite(&key_iter->first, sizeof(key_iter->first), 1, fp);
//        fwrite(&key_iter->second.destructor_ptr,
//               sizeof(key_iter->second.destructor_ptr),
//               1,
//               fp);
//
//        inner_count = key_iter->second.thread_values.size();
//        fwrite(&inner_count, sizeof(inner_count), 1, fp);
//        for (thr_val_iter = key_iter->second.thread_values.begin();
//             thr_val_iter != key_iter->second.thread_values.end();
//             thr_val_iter++) {
//            fwrite(&thr_val_iter->first,
//                   sizeof(thr_val_iter->first),
//                   1,
//                   fp);
//            fwrite(&thr_val_iter->second,
//                   sizeof(thr_val_iter->second),
//                   1,
//                   fp);
//        }
//    }
//
//
//    // system.barriers //////////////////////////////////////////////
//    count = system.barriers.size();
//    fwrite(&count, sizeof(count), 1, fp);
//    if (about) 
//        fprintf(about, "system barrier objects: %" PRIu64"\n", count);
//    for (barrier_iter = system.barriers.begin();
//         barrier_iter != system.barriers.end();
//         barrier_iter++) {
//        fwrite(&barrier_iter->first, sizeof(barrier_iter->first), 1, fp);
//        
//        fwrite(&barrier_iter->second.threads_required, 
//               sizeof(barrier_iter->second.threads_required), 
//               1, 
//               fp);
//        fwrite(&barrier_iter->second.threads_blocked, 
//               sizeof(barrier_iter->second.threads_blocked), 
//               1, 
//               fp);
//        
//        uint64_t n, tid;
//        inner_count = barrier_iter->second.blocked_threads.size();
//        fwrite(&inner_count, sizeof(inner_count), 1, fp);
//        for (n = 0; n < inner_count; n++) {
//            tid = barrier_iter->second.blocked_threads[n]->pthread_tid;
//            fwrite(&tid, sizeof(tid), 1, fp);
//        }
//    }
//
//
//    // system.conds /////////////////////////////////////////////////
//    count = system.conds.size();
//    fwrite(&count, sizeof(count), 1, fp);
//    if (about) 
//        fprintf(about, "system cond objects: %" PRIu64"\n", count);
//    for (cond_iter = system.conds.begin();
//         cond_iter != system.conds.end();
//         cond_iter++) {
//        fwrite(&cond_iter->first, sizeof(cond_iter->first), 1, fp);
//
//        uint64_t n, tid;
//        inner_count = cond_iter->second.blocked_threads.size();
//        fwrite(&inner_count, sizeof(inner_count), 1, fp);
//        for (n = 0; n < inner_count; n++) {
//            fwrite(&cond_iter->second.blocked_threads[n].mutex_ptr,
//                   sizeof(cond_iter->second.blocked_threads[n].mutex_ptr),
//                   1,
//                   fp);
//
//            tid = cond_iter->second.blocked_threads[n].thread->pthread_tid;
//            fwrite(&tid, sizeof(tid), 1, fp);
//        }
//    }
//
//    // flush the about file
//    if (about) 
//        fflush(about);
//}
//
///////////////////////////////////////////////////////////////////////////////
//
//
//void Process::load_current_system_state_from_checkpoint(FILE *fp, bool apply)
//{
//    uint64_t i, j, count, inner_count, first;
//    int64_t join;
//    map<int64_t, MIPSSystemState_FD>::iterator fd_iter;
//
//    // read the alpha system state
//    fread(&system.brk_point, sizeof(system.brk_point), 1, fp);
//    fread(&system.mmap_end, sizeof(system.mmap_end), 1, fp);
//    fread(&system.next_fd_translation, sizeof(system.next_fd_translation), 1, fp);
//    fread(&system.next_stack_base, sizeof(system.next_stack_base), 1, fp);
//    fread(&system.threads_spawned, sizeof(system.threads_spawned), 1, fp);
//    fread(&system.next_key, sizeof(system.next_key), 1, fp);
//
//    // reading the pthread stuff can get dicey.  complex ...
//
//    // first, reset the maps
//    // FIXME some of these will probably leak memory.
//    system.pthreads.clear();
//    system.mutexes.clear();
//    system.keys.clear();
//    system.barriers.clear();
//    system.conds.clear();
//
//    // close open file descriptors 
//    for (fd_iter = system.fd_translation.begin();
//         fd_iter != system.fd_translation.end();
//         fd_iter++) {
//        // only close it if it's not a system fd
//        if (!fd_iter->second.system_fd)
//            close(fd_iter->second.fd_simulator);
//    }
//    system.fd_translation.clear();
//
//    // system.fd_translation ////////////////////////////////////////
//    fread(&count, sizeof(count), 1, fp); //
//    if (apply)
//        DEBUG_CHECKPOINTS("%lu fd objects\n", count);
//    for (i = 0; i < count; i++) {
//        int64_t fd_first;
//        int64_t offset;
//
//        fread(&fd_first, sizeof(fd_first), 1, fp); //
//        fread(system.fd_translation[fd_first].pathname, //
//              sizeof(char),
//              SYSCALL_BUFFER_SIZE,
//              fp);
//        fread(&system.fd_translation[fd_first].local_flags, //
//              sizeof(system.fd_translation[fd_first].local_flags),
//              1,
//              fp);
//        fread(&system.fd_translation[fd_first].mode, //
//              sizeof(system.fd_translation[fd_first].mode),
//              1,
//              fp);
//        fread(&system.fd_translation[fd_first].system_fd, //
//              sizeof(system.fd_translation[fd_first].system_fd),
//              1,
//              fp);
//        fread(&system.fd_translation[fd_first].system_fd_number, //
//              sizeof(system.fd_translation[fd_first].system_fd_number),
//              1,
//              fp);
//        fread(&offset, sizeof(offset), 1, fp); //
//
//        // don't re-open system fd's, just point to them 
//        if (system.fd_translation[fd_first].system_fd) {
//            system.fd_translation[fd_first].fd_simulator =
//                system.fd_translation[fd_first].system_fd_number;
//        }
//        // re-open non-system fd's.
//        else {
//            int64_t fd_simulator = 
//                open(system.fd_translation[fd_first].pathname,
//                     system.fd_translation[fd_first].local_flags,
//                     system.fd_translation[fd_first].mode);
//
//            system.fd_translation[fd_first].fd_simulator = fd_simulator;
//            if (fd_simulator == -1)
//                fatal("could not re-open file descriptor!  path: %s",
//                      system.fd_translation[fd_first].pathname);
//
//            // move the offset if appropriate
//            if (offset != -1) {
//                int64_t new_offset;
//
//                new_offset = lseek(fd_simulator, offset, SEEK_SET);
//                if (new_offset != offset)
//                    fatal("unable to move offset back in file: %s\n",
//                          system.fd_translation[fd_first].pathname);
//            }
//        }
//    }
//
//    // system.pthreads //////////////////////////////////////////////
//    fread(&count, sizeof(count), 1, fp);
//    if (apply)
//        DEBUG_CHECKPOINTS("%lu pthread objects\n", count);
//    for (i = 0; i < count; i++) {
//        fread(&first, sizeof(first), 1, fp);
//        fread(&system.pthreads[first].complete,
//              sizeof(system.pthreads[first].complete),
//              1,
//              fp);
//        fread(&system.pthreads[first].retval,
//              sizeof(system.pthreads[first].retval),
//              1,
//              fp);
//        fread(&join, sizeof(join), 1, fp);
//        if (join == -1)
//            system.pthreads[first].blocked_join_thread = NULL;
//        else
//            system.pthreads[first].blocked_join_thread = thread_vector[join];
//        fread(&system.pthreads[first].join_retval_ptr,
//              sizeof(system.pthreads[first].join_retval_ptr),
//              1,
//              fp);
//        fread(&system.pthreads[first].attr,
//              sizeof(system.pthreads[first].attr),
//              1,
//              fp);
//    }
//
//    // system.mutexes ///////////////////////////////////////////////
//    fread(&count, sizeof(count), 1, fp);
//    if (apply)
//        DEBUG_CHECKPOINTS("%lu mutex objects\n", count);
//    for (i = 0; i < count; i++) {
//        int64_t owner = -1;
//        int64_t tid;
//
//        fread(&first, sizeof(first), 1, fp);
//        fread(&owner, sizeof(owner), 1, fp);
//        if (owner == -1)
//            system.mutexes[first].owner = NULL;
//        else
//            system.mutexes[first].owner = thread_vector[owner];
//
//        fread(&inner_count, sizeof(inner_count), 1, fp);
//        if (owner == -1 &&
//            inner_count > 0)
//            fatal("not expecting blocked threads on unowned mutexes.");
//        for (j = 0; j < inner_count; j++) {
//            fread(&tid, sizeof(tid), 1, fp);
//            if ((tid < 0) || (tid >= (int64_t)thread_vector.size()))
//                fatal("unexpected mutex blocked thread tid %li", tid);
//            system.mutexes[first].blocked_threads.push_back(
//                thread_vector[tid]);
//
//            if (!thread_vector[tid]->blocked)
//                fatal("expected this thread to be blocked!");
//        }
//    }
//
//    // system.keys //////////////////////////////////////////////////
//    fread(&count, sizeof(count), 1, fp);
//    if (apply)
//        DEBUG_CHECKPOINTS("%lu key objects\n", count);
//    for (i = 0; i < count; i++) {
//        fread(&first, sizeof(first), 1, fp);
//        fread(&system.keys[first].destructor_ptr,
//              sizeof(system.keys[first].destructor_ptr),
//              1,
//              fp);
//
//        fread(&inner_count, sizeof(inner_count), 1, fp);
//
//        for (j = 0; j < inner_count; j++) {
//            uint64_t inner_first, inner_second;
//            fread(&inner_first,
//                  sizeof(inner_first),
//                  1,
//                  fp);
//            fread(&inner_second,
//                  sizeof(inner_second),
//                  1,
//                  fp);
//            system.keys[first].thread_values[inner_first] = inner_second;
//        }
//    }
//
//    // system.barriers //////////////////////////////////////////////
//    fread(&count, sizeof(count), 1, fp);
//    if (apply)
//        DEBUG_CHECKPOINTS("%lu barrier objects\n", count);
//    for (i = 0; i < count; i++) {
//        fread(&first, sizeof(first), 1, fp);
//        fread(&system.barriers[first].threads_required,
//              sizeof(system.barriers[first].threads_required),
//              1, 
//              fp);
//        fread(&system.barriers[first].threads_blocked,
//              sizeof(system.barriers[first].threads_blocked),
//              1, 
//              fp);
//
//        fread(&inner_count, sizeof(inner_count), 1, fp);
//        for (j = 0; j < inner_count; j++) {
//            uint64_t tid;
//            fread(&tid, sizeof(tid), 1, fp);
//            if (tid >= thread_vector.size())
//                fatal("unexpected mutex blocked thread tid %li", tid);
//            system.barriers[first].blocked_threads.push_back(
//                thread_vector[tid]);
//        }
//    }
//
//    // system.conds /////////////////////////////////////////////////
//    fread(&count, sizeof(count), 1, fp);
//    if (apply)
//        DEBUG_CHECKPOINTS("%lu cond objects\n", count);
//    for (i = 0; i < count; i++) {
//        fread(&first, sizeof(first), 1, fp);
//        fread(&inner_count, sizeof(inner_count), 1, fp);
//        for (j = 0; j < inner_count; j++) {
//            uint64_t tid;
//
//            MIPSSystemState_CondDeque cd;
//
//            fread(&cd.mutex_ptr,
//                  sizeof(cd.mutex_ptr),
//                  1,
//                  fp);
//            fread(&tid, sizeof(tid), 1, fp);
//            if (tid >= thread_vector.size())
//                fatal("unexpected mutex blocked thread tid %li", tid);
//            cd.thread = thread_vector[tid];
//
//            system.conds[first].blocked_threads.push_back(cd);
//        }
//    }
//}
//
///////////////////////////////////////////////////////////////////////////////

//void
//Process::reduce_checkpoint(const char *full_checkpoint,
//                           const char *reduced_checkpoint)
//{
//    // This function is intended to load a full checkpoint (all regions) and
//    // systematically delete memory from the checkpoint that aren't accessed
//    // during a functional simulation of the checkpointed regions.  This
//    // process loses data, however, the data lost only has the potential to be
//    // accessed down a wrong path, and in the cases where this occurs it is
//    // generally going to be a cache miss that resolves after the path is
//    // corrected.  Furthermore, the lines are marked on somewhat of a wide
//    // coarse, so data nearby accessed data is included.
//    //
//    // Copy the high-level checkpoint files.
//    // Loop around the checkpoint regions.  For each region:
//    //     Save the memory at the beginning of the previous reduced checkpoint
//    //     Load the region from the pristine checkpoint
//    //     Simulate and mark all touched memory within region
//    //     Save the differences between the reduced checkpoints, only for 
//    //        portions touched within the region
//    //
//    Memory *beginning_last_checkpoint_reduced;
//    Memory *beginning_this_checkpoint_reduced;
//    vector<Thread*>::iterator thread_iter;
//    Thread* T;
//    FILE *fp_loader;
//    FILE *fp_about;
//    FILE *fp_checkpoint;
//    FILE *fp_temp;
//    char temp_filename[80];
//    char *buffer;
//    unsigned int num, len, len2;
//    long int original_size, new_size;
//    long int total_original_size, total_new_size;
//    double ratio;
//
//    // initialize total file sizes to 0
//    total_original_size = 0;
//    total_new_size = 0;
//
//    // open about file for writing
//    sprintf(temp_filename, "%s.about", reduced_checkpoint);
//    fprintf(out, "opening file \"%s\" for writing\n", temp_filename);
//    fp_about = fopen(temp_filename, "w");
//
//    // write additional info to the about file
//    fprintf(fp_about, "MEMORY REDUCED CHECKPOINT\n");
//    fprintf(fp_about, "this checkpoint name:     %s\n", reduced_checkpoint);
//    fprintf(fp_about, "original checkpoint name: %s\n", full_checkpoint);
//    fprintf(fp_about, "\n");
//
//    // append the original about file to this about file
//    sprintf(temp_filename, "%s.about", full_checkpoint);
//    fp_temp = fopen(temp_filename, "r");
//    if (fp_temp) {
//        fseek(fp_temp, 0 , SEEK_END);
//        len = ftell(fp_temp);
//        rewind(fp_temp);
//
//        buffer = (char*)malloc(sizeof(char)*len);
//        if (!buffer) fatal("Out of memory");
//
//        len2 = fread(buffer, 1, len, fp_temp);
//        if (len2 != len) fatal("Read error");
//        fclose(fp_temp);
//
//        fwrite(buffer, 1, len, fp_about);
//        fflush(fp_about);
//
//        free(buffer);
//    }
//    fprintf(fp_about, "\n");
//
//    // open the loader datafile for writing
//    sprintf(temp_filename, "%s", reduced_checkpoint);
//    fprintf(out, "opening file \"%s\" for writing\n", temp_filename);
//    fp_loader = fopen(temp_filename, "w");
//    
//    // copy the old loader datafile to the new loader datafile
//    sprintf(temp_filename, "%s", full_checkpoint);
//    fp_temp = fopen(temp_filename, "r");
//    if (fp_temp) {
//        fseek(fp_temp, 0 , SEEK_END);
//        len = ftell(fp_temp);
//        rewind(fp_temp);
//
//        buffer = (char*)malloc(sizeof(char)*len);
//        if (!buffer) fatal("Out of memory");
//
//        len2 = fread(buffer, 1, len, fp_temp);
//        if (len2 != len) fatal("Read error");
//        fclose(fp_temp);
//
//        fwrite(buffer, 1, len, fp_loader);
//        free(buffer);
//    }
//    else {
//        fatal("couldn't open the old loader?");
//    }
//    
//    // close loader datafile
//    fclose(fp_loader);
//    
//    // allocate new memory structures
//    beginning_last_checkpoint_reduced = new Memory();
//    beginning_this_checkpoint_reduced = new Memory();
//
//    // turn on memory access tracking
//    memory->enable_tracking();
//
//    // loop over all of the checkpoints
//    num = 1;
//    while (load_checkpoint(full_checkpoint, num)) {
//        
//        // open checkpoint file
//        sprintf(temp_filename, "%s.%04i", reduced_checkpoint, num);
//        fprintf(out, "saving \"%s\". ", temp_filename);
//        fflush(stdout);
//        fp_checkpoint = fopen(temp_filename, "w");
//
//        // begin writing checkpoint here ////////////////////////////
//        // write number dynamic instructions from beginning of program
//        // get it from the original checkpoint file, since we've cleared it
//        original_size = 0;
//        sprintf(temp_filename, "%s.%04i", full_checkpoint, num);
//        fp_temp = fopen(temp_filename, "r");
//        if (fp_temp) {
//            fread(&popped_inst_count, sizeof(popped_inst_count), 1, fp_temp);
//            fseek(fp_temp, 0 , SEEK_END);
//            original_size = ftell(fp_temp);
//            fclose(fp_temp);
//        }
//        fwrite(&popped_inst_count, sizeof(popped_inst_count), 1, fp_checkpoint);
//        popped_inst_count = 0;
//
//        // write the number of threads in the system
//        uint32_t num_threads = thread_vector.size();
//        fwrite(&num_threads, sizeof(num_threads), 1, fp_checkpoint);
//
//        // write the threads
//        for (thread_iter = thread_vector.begin();
//             thread_iter != thread_vector.end();
//             thread_iter++) {
//            T = *thread_iter;
//
//            // do not write the thread name
//
//            // write the thread block and complete data
//            fwrite(&T->blocked, sizeof(T->blocked), 1, fp_checkpoint);
//            fwrite(&T->block_reason, 
//                   sizeof(T->block_reason), 
//                   1, 
//                   fp_checkpoint);
//            fwrite(&T->complete, sizeof(T->complete), 1, fp_checkpoint);
//
//            // write the stack data
//            fwrite(&T->stack_base, sizeof(T->stack_base), 1, fp_checkpoint);
//            fwrite(&T->stack_min, sizeof(T->stack_min), 1, fp_checkpoint);
//            
//            // write architectural state 
//            fwrite(&T->context.pc, sizeof(T->context.pc), 1, fp_checkpoint);
//            fwrite(T->context.regs, sizeof(T->context.regs[0]), NUM_REGS, fp_checkpoint);
//
//            // write the retired instruction count
//            fwrite(&T->num_insts, sizeof(T->num_insts), 1, fp_checkpoint);
//            
//            // write the pthread id
//            fwrite(&T->pthread_tid, sizeof(T->pthread_tid), 1, fp_checkpoint);
//        }
//
//        // save the system state, but do not write the about file (clutter).
//        save_current_system_state_to_checkpoint(fp_checkpoint, NULL);
//    
//        // reset the memory tracking to a blank state
//        memory->reset_tracking();
//
//        // start by cloning the memory from the simulator, so we can save it at
//        // the end of the checkpoint region.
//        beginning_this_checkpoint_reduced->clone(memory);
//        
//        // simulate the region and mark the memory accesses
//        fprintf(out, "simulating. ");
//        fflush(stdout);
//        thread_iter = thread_vector.begin();
//        while (!program_complete) {
//            bool was_trapped = false;
//
//            // skip exactly one instruction from this thread, if eligible
//            T = *thread_iter;
//            if (!T->blocked) {
//                // get next instruction
//                arch_inst_t *inst = T->pop_inst_debug_queue();
//                if (inst->is_trap)  {
//                    // handle the trap
//                    T->handle_trap();
//
//                    // cleanup thread if it has completed
//                    if (T->complete)
//                        T->cleanup_complete_thread();
//
//                    // traps potentially break the thread iterator, so reset it
//                    thread_iter = thread_vector.begin();
//                }
//            }
//
//            if (!was_trapped) {
//                thread_iter++;
//                if (thread_iter == thread_vector.end())
//                    thread_iter = thread_vector.begin();
//            }
//        }
//
//        // the region is over, reduce the memory region now
//        beginning_this_checkpoint_reduced->reduce_memory_using_accesses(memory);
//
//        // merge in the previous reduced state to the current reduced state
//        // remember, the intent is to reduce checkpoint filesize, not
//        // simulator memory state size
//        beginning_this_checkpoint_reduced->merge_older_partial_memory_map(
//            beginning_last_checkpoint_reduced);
//
//        // save memory map here!
//        fprintf(out, "writing reduced memory map. ");
//        fflush(stdout);
//        beginning_this_checkpoint_reduced->write_diffs_to_file(
//            fp_checkpoint, 
//            beginning_last_checkpoint_reduced);
//
//        // clone the beginning checkpoint to the last one so the diffs
//        // go forward correctly.
//        beginning_last_checkpoint_reduced->clone(
//            beginning_this_checkpoint_reduced);
//
//        // done writing checkpoint here /////////////////////////////
//        
//        // print out size of file and checkpoint
//        new_size = ftell(fp_checkpoint);
//        ratio = (double)100.0 - ((double)new_size / (double)original_size * (double)100.0);
//        fprintf(out, "%li bytes. (%4.2f%% reduced)\n", new_size, ratio);
//        fflush(stdout);
//
//        // write size data to about file
//        fprintf(fp_about, "checkpoint %u memory reduction\n", num);
//        fprintf(fp_about, "original checkpoint size: %li\n", original_size);
//        fprintf(fp_about, "new checkpoint size:      %li\n", new_size);
//        fprintf(fp_about, "reduction:                %4.2f%%\n", ratio);
//        fprintf(fp_about, "\n");
//
//        // update total file size running totals
//        total_original_size += original_size;
//        total_new_size += new_size;
//        
//        // close checkpoint file
//        fclose(fp_checkpoint);
//
//        // increment checkpoint number
//        num++;
//    }
//        
//    // write total size data to about file
//    ratio = (double)100.0 - ((double)total_new_size / (double)total_original_size * (double)100.0);
//    fprintf(fp_about, "TOTAL CHECKPOINT MEMORY REDUCTION\n");
//    fprintf(fp_about, "total original checkpoint size: %li\n", total_original_size);
//    fprintf(fp_about, "total new checkpoint size:      %li\n", total_new_size);
//    fprintf(fp_about, "total reduction:                %4.2f%%\n", ratio);
//    fprintf(fp_about, "\n");
//
//    // close about file
//    fclose(fp_about);
//}

/////////////////////////////////////////////////////////////////////////////


