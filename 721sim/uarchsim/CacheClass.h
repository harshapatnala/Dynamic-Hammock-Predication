#ifndef DCACHE_H
#define DCACHE_H
/*--------------------------------------------------------------------------*\
 | CacheClass.h
 | Timothy Heil
 | 1/23/96
 |
 | Provides D-Cache interface for simulator.
 | Currently, D-Cache keeps only time-stamps and does not actually cache
 |  the data.
 |
 | Adjustable Cache parameters:
 |  Cache Line Size
 |  Cache Associativity
 |  Cache Size
 |  Cache Access Latency
 |  Cache Miss Latency
 |  Number of outstanding misses
 |  Number of ports to backing store
 |  Backing store port reuse latency
 |
 | Fixed cache parameters:
 |  Replacement policy (LRU)
 |  Write policy (Write Back)
 |  Number of cache ports (unlimited)
\*--------------------------------------------------------------------------*/
#include "decode.h"
#include "cache.h"
#include "histogram.h"
#include <string.h>

/*--------------------------------------------------------------------------*\
 | Miss Handleing Status Register provides multiple outstanding reads and
 |  writes.  Collapses requests for the same line.
\*--------------------------------------------------------------------------*/
class MHSRClass {
public:
	reg_t lineAddress;    /* Line being loaded by MHSR.        */
	int64_t   resolved;       /* When miss will be completed.      */
  bool  busy;          /*Whether MHSR is busy */
};

/*--------------------------------------------------------------------------*\
 | State maintained by a D-Cache line.
\*--------------------------------------------------------------------------*/
class CacheLineClass {
public:
	int mhsr;   /* Index of MHSR that is loading this line.        */
	bool mhsrValid; /* -1 indicates that the line is not being loaded. */
	bool dirty; /* Indicates the line is dirty.                    */
};

typedef cache<CacheLineClass> CacheArray;

//Forward declaring class
class pipeline_t;
class stats_t;

class CacheClass {
public:
	CacheClass(int sets, int assoc, int _lineSize,
	           int _hitLatency, int _missLatency,
	           int _numMHSR, int _numMissSrvPorts, int _missSrvLatency,
	           pipeline_t* _proc, const char* _identifier,
             CacheClass* _nextLevel=NULL, int histLen = 50);
	/*------------------------------------------------------------------------*\
	 | Constructor.  Allocates data structures and initializes D-cache state.
	 |
	 |  sets           The number of sets in the cache.
	 |  assoc          The number of ways per set (associativity).
	 |  lineSize       The log2 of the size of a line in bytes.
	 |                  (A line is  (1<<lineSize) bytes long.
	 |  hitLatency     The number of cycles required to process a hit
	 |                  to the D-Cache.  Should be at least 1.
	 |  missLatency    The number of cycles required to process a miss
	 |                  to the D-Cache.  This is also used as the number
	 |                  of cycles to flush a dirty line to memory.
	 |  numMHSRs       The number of miss handleing status registers.
	 |                  (Maximum number of outstanding cache misses. )
	 |  missSrvPorts   The number of misses that can be serviced in a single
	 |                  cycle (number of ports to the backing store.)
	 |  missSrvLat     The number of cycles before a miss service port can be
	 |                  reused (port pipeline latency).
	\*------------------------------------------------------------------------*/

	~CacheClass();
	/*------------------------------------------------------------------------*\
	 | Destructor.  Frees memory used by D-cache.
	\*------------------------------------------------------------------------*/

	void flush();
  void set_lat(unsigned int hit_lat, unsigned int miss_lat);  // ER 06/19/01
  void set_log_file(FILE* file_ptr);

	cycle_t Access(unsigned int Tid /* ER 11/16/02 */,
	               cycle_t curCycle, reg_t addr, bool isStore,
	               bool* hit=NULL, bool probe=false, bool commit=true);
	/*------------------------------------------------------------------------*\
	 | Access the data cache.  Determines how many cycles access will take.
	 |
	 |  curCycle          The current simulation cycle.  This must always
	 |                     increase or be the same, from call to call.
	 |  addr              The address of the word being accessed.
	 |  isStore           Indicates whether the access is a store (true) or
	 |                     load (false).
	 |
	 | Returns the cycle when the access will complete.  Returns -1 if the
	 |  access can not be handled, due to limited miss handleing status
	 |  registers.
	\*------------------------------------------------------------------------*/

	bool Probe(unsigned int Tid,cycle_t curCycle, reg_t addr1, unsigned int length);
	HistogramClass* accessLatency;
	void set_nextLevel(CacheClass* nLevel);
private:

  pipeline_t* proc;
	int FindFreeMHSR(cycle_t curCycle);
	int FindNextPort(cycle_t curCycle, cycle_t* portAvail);

	CacheArray  array;          /* The D-Cache array.                           */
  CacheClass* nextLevel; 
  std::string identifier;
	int         lineSize;        /* D-Cache line size.  Must be a power of 2.    */
//	cycle_t     lastCycle;         /* curCycle of last access.                     */
	cycle_t     hitLatency;        /* Cycles of delay for cache hits.              */
	cycle_t     missLatency;       /* Cycles of delay for cache misses, and writeibacks */
	cycle_t*    missPortAvail;    /* Cycle each miss port will be available.
                              *  Currently these ports are used for write-
                              *  backs as well.                              */
	int         numMHSR;         /* The number of MHSRs.                         */
	MHSRClass*  mhsr;           /* The miss handling status registers.          */
	int         numMissSrvPorts;       /* Number of miss ports available.              */
	cycle_t     missSrvLatency;    /* Pipeline reuse latency for miss ports.       */

  stats_t* stats;

};

#endif //DCACHE_H
