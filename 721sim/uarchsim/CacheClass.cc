#pragma implementation "cache.h"
/*--------------------------------------------------------------------------*\
 | CacheClass.cc
 | Timothy Heil
 | 1/23/96
 |
 | Heavily modified by 
 | Rangeen Basu Roy Chowdhury
 |
 | Provides Cache interface for simulator.
\*--------------------------------------------------------------------------*/

#include <cstdlib>
#include <cassert>

#include "histogram.h"
#include "cache.h"
#include "CacheClass.h"
#include "pipeline.h"
#include "stats.h"
#include "parameters.h"

CacheClass::CacheClass(int sets, int assoc, int _lineSize,
                       int _hitLatency, int _missLatency,
                       int _numMHSR, int _numMissSrvPorts,  int _missSrvLatency,
                       pipeline_t* _proc, const char* _identifier, 
                       CacheClass* _nextLevel, int histLen)
	: proc(_proc),
    array(sets, assoc),  // Allocate cache array.
    nextLevel(_nextLevel),
    lineSize(_lineSize),
    hitLatency(_hitLatency),
    missLatency(_missLatency),
	  numMHSR(_numMHSR),
	  numMissSrvPorts(_numMissSrvPorts),
	  missSrvLatency(_missSrvLatency)
	  /*------------------------------------------------------------------------*\
	   | Constructor.  Allocates data structures and initializes D-cache state.
	   |
	   |  sets           The number of sets in the cache.
	   |  assoc          The number of ways per set (associativity).
	   |  hitLatency     The number of cycles required to process a hit
	   |                  to the D-Cache.  Should be at least 1.
	   |  missLatency    The number of cycles required to process a miss
	   |                  to the D-Cache.  This is also used as the number
	   |                  of cycles to flush a dirty line to memory.
	   |  numMHSR        The number of miss handleing status registers.
	   |                  (Maximum number of outstanding cache misses. )
	   |  numMissSrvPorts   The number of misses that can be serviced in a single
	   |                  cycle (number of ports to the backing store.)
	   |  missSrvLatency     The number of cycles before a miss service port can be
	   |                  reused (port pipeline latency).
	  \*------------------------------------------------------------------------*/
{
	int i;

  identifier = _identifier;

	/* Set cache parameters. */
//	lastCycle      = 0;

	/* Allocate MHSRs */
	mhsr = new MHSRClass[numMHSR];
	assert(mhsr);
	for (i=0; i<numMHSR; i++) {
		mhsr[i].resolved = 0;
		mhsr[i].busy = false;
	}

	/* Allocate miss service ports. */
	missPortAvail = new cycle_t[numMissSrvPorts];
	assert(missPortAvail);
	for (i=0; i<numMissSrvPorts; i++) {
		missPortAvail[i]=0;
	}

  this->stats = proc->get_stats();

  assert(stats);

#if 0
  stats->register_counter((identifier+"_load_count").c_str()        ,identifier.c_str());
  stats->register_counter((identifier+"_store_count").c_str()       ,identifier.c_str());
  stats->register_counter((identifier+"_load_hit_count").c_str()    ,identifier.c_str());
  stats->register_counter((identifier+"_store_hit_count").c_str()   ,identifier.c_str());
  stats->register_counter((identifier+"_load_miss_count").c_str()   ,identifier.c_str());
  stats->register_counter((identifier+"_store_miss_count").c_str()  ,identifier.c_str());
  stats->register_counter((identifier+"_read_access_count").c_str() ,identifier.c_str());
  stats->register_counter((identifier+"_write_access_count").c_str(),identifier.c_str());

  if(verbose_phase_counters){
    stats->register_phase_counter((identifier+"_load_count").c_str()        ,identifier.c_str());
    stats->register_phase_counter((identifier+"_store_count").c_str()       ,identifier.c_str());
    stats->register_phase_counter((identifier+"_load_hit_count").c_str()    ,identifier.c_str());
    stats->register_phase_counter((identifier+"_store_hit_count").c_str()   ,identifier.c_str());
    stats->register_phase_counter((identifier+"_load_miss_count").c_str()   ,identifier.c_str());
    stats->register_phase_counter((identifier+"_store_miss_count").c_str()  ,identifier.c_str());
    stats->register_phase_counter((identifier+"_read_access_count").c_str() ,identifier.c_str());
    stats->register_phase_counter((identifier+"_write_access_count").c_str(),identifier.c_str());
  }
#endif

}

void CacheClass::flush()
{
	array.flush();
}

CacheClass::~CacheClass()
/*------------------------------------------------------------------------*\
 | Destructor.  Frees memory used by D-cache.
\*------------------------------------------------------------------------*/
{
	delete [] mhsr;
	delete [] missPortAvail;

}

cycle_t CacheClass::Access(unsigned int Tid /* ER 11/16/02 */,
                             cycle_t curCycle, reg_t addr,
                             bool isStore, bool* isHit,
                             bool probe, bool commit)
/*------------------------------------------------------------------------*\
 | Access the data cache.  Determines how many cycles access will take.
 |
 |  curCycle          The current simulation cycle.  This must always
 |                     increase or be the same, from call to call. This
 |                    indicates the cycle in which the cache is being 
 |                    accessed and could be in the future. This is especially
 |                    inportant for L2 cache accesses where the L2 will be
 |                    accessed only after MHSR and miss ports have been allocated
 |                    and this can be in the future.
 |  addr              The address of the word being accessed.
 |  isStore           Indicates whether the access is a store (true) or
 |                     load (false).
 |
 | Returns the cycle when the access will complete.  Returns -1 if the
 |  access can not be handled, due to limited miss handleing status
 |  registers.
\*------------------------------------------------------------------------*/
{
	bool hit;
	reg_t lineAddr;
	reg_t oldAddr;
	CacheLineClass* line;
	CacheLineClass* newLine;
	int busyMHSR;
	int newMHSR;
	int newPort;
	cycle_t portAvail;
	cycle_t lineInArray;

//	assert (curCycle >= lastCycle);
//	lastCycle = curCycle;

	// ER 11/16/02
	//lineAddr = addr >> lineSize;
	assert((Tid < 4) && (lineSize >= 2));
	lineAddr = ((addr >> lineSize) | (Tid << 30));

	line = array.lookup(lineAddr, NULL, &hit, &oldAddr, false);

	if (probe) {
		(*isHit) = hit;
		return(curCycle);
	}

  if(isStore){
    inc_counter_str((identifier+"_store_count").c_str());
  } else {
    inc_counter_str((identifier+"_load_count").c_str());
  }
  // Line has been allocated in cache.
	if (hit) {

		// Check if line is being dirtied.
		if ((isStore) && (!line->dirty)) {
			// Line must be changed to dirty.
			line->dirty = true;
		}

		// Check if line is currently being loaded (is busy).
		busyMHSR = line->mhsr;
		if (busyMHSR != -1) {

			// Line might be busy.
			// Check MHSR.
			// Cache line was already loaded.
			if (mhsr[busyMHSR].resolved <= curCycle) {
				lineInArray = curCycle;
				line->mhsr = -1;
				mhsr[busyMHSR].resolved = 0;
        		mhsr[busyMHSR].busy = false;
			}
			// Cache line is being loaded.
			else {
				lineInArray = mhsr[busyMHSR].resolved;
				// For lines that are being loaded now, must 
        // wait at least one cache check hit time 
        // after the miss has been resolved.
				if ((lineInArray - curCycle) < hitLatency) {
					lineInArray = curCycle + hitLatency;
				}
			}
		}
		// Line is already available in cache.
		else {
			//lineInArray = curCycle + hitLatency;
			lineInArray = curCycle;
      if(isStore){
        inc_counter_str((identifier+"_store_hit_count").c_str());
        inc_counter_str((identifier+"_write_access_count").c_str());
      } else {
        inc_counter_str((identifier+"_load_hit_count").c_str());
        inc_counter_str((identifier+"_read_access_count").c_str());
      }
		}
	}
  // Line has not been allocated in cache.
	else {

    if(isStore){
      inc_counter_str((identifier+"_store_miss_count").c_str());
    } else {
      inc_counter_str((identifier+"_load_miss_count").c_str());
    }

		// Allocate MHSR to handle cache miss.
    // Return error value if no free MHSR
    // is found. The previous level will
    // retry later.
		newMHSR = FindFreeMHSR(curCycle);
		if (newMHSR == -1) {
		   if (isHit != NULL)
		      (*isHit) = false;

		   return(-1);
		}
		//if (newMHSR == -1) return(-1);
		//if (newMHSR == -1) {
		//	assert(0);
		//}

		// Find the miss port to use for handling the miss.
		newPort = FindNextPort(curCycle, &portAvail);

		// Allocate a new cache line structure.
		if (commit) {
			// Allocate a new cache line structure.
			newLine = new CacheLineClass;
			assert(newLine);
			newLine -> mhsr = newMHSR;
			newLine -> dirty = isStore;

			// Replace the old line in the cache.
			line = array.lookup(lineAddr, newLine, &hit, &oldAddr, true);
		}

		// Compute the time to load the new line from the next memory level.
    // Added curCycle: RBRC 04/04/2015
		lineInArray = (portAvail < curCycle) ? curCycle : portAvail;
		//lineInArray = portAvail;

		// Must wait for hit latency cycles before beginning line load.
		if ((lineInArray-curCycle) < hitLatency) {
			lineInArray = curCycle + hitLatency;
		}

		// See if line being replaced is itself still being loaded.
		if (commit && (line !=NULL)) {
			busyMHSR = line->mhsr;
			if (busyMHSR != -1) {
				// Line being replaced is being loaded.  Must wait until this
				//  line has been loaded to replace it.
				//  NOTE: There is a slight simulation approximation made here.
				//        The miss port is being tied up for the entire time,
				//        but will not actually be used until later.
				if (mhsr[busyMHSR].resolved > lineInArray) {
					lineInArray = mhsr[busyMHSR].resolved;
				}
			}

			// See if line is dirty.  Line must be written back, if dirty.
			if (line->dirty) {
        inc_counter_str((identifier+"_read_access_count").c_str());
        if(nextLevel == NULL){
				  lineInArray = lineInArray + missLatency;
        } else {
          // lineInArray is when the next level access will start.
          // The next level does its calculation assuming lineinArray
          // as it's access cycle and returns when the line becomes 
          // available for access.
          // Must wait for writeBack to be acknowledged, which happens
          // after accessing the next level. It is assumed that writeback
          // uses a seprate port to next level than the allocate port.
				  lineInArray = nextLevel->Access(Tid,lineInArray,addr,true,&hit);
          assert(lineInArray > curCycle);
        }
			}
		}

		// Allocate miss port.
		missPortAvail[newPort] = lineInArray + missSrvLatency;

		// Add miss latency to access time.
    if(nextLevel == NULL){
  		lineInArray = lineInArray + missLatency;
    } else {
      // lineInArray is when the next level access will start.
      // The next level does its calculation assuming lineinArray
      // as it's access cycle and returns when the line becomes 
      // available for access.
      // This is always a read from the next level as this is a WBWA cache model. 
  		lineInArray = nextLevel->Access(Tid,lineInArray,addr,false,&hit);
      // Cannot miss in MHSR in the next level if the next level has
      // as many or more MHSRs as this level. A miss in this level can
      // be a hit or a miss in the next level. There can be numMHSR outstanding 
      // misses in this level and hence fewer than or equal to numMHSR misses
      // in the next level.
      assert(lineInArray > curCycle);
    }

		// Free old cache line, allocate miss port and MHSR.
		// NOTE: Slight simulation approximation error here.
		//       MHSR is being allocated this cycle, but in reality, can not
		//       be allocated until hitLat cycles later, when miss is
		//       known.
		if (commit && line) {
			delete line;
		}
		mhsr[newMHSR].resolved = lineInArray;
		mhsr[newMHSR].busy = true;
		mhsr[newMHSR].lineAddress = lineAddr;
    inc_counter_str((identifier+"_write_access_count").c_str());
	}

	if (isHit!=NULL) {
		(*isHit) = (lineInArray == curCycle);
	}

  //LOG(proc->lsu_log,proc->cycle,uint64_t(0),uint64_t(0),"Executed %s which %s resolve cycle %" PRIcycle "",isStore?"store":"load",isHit?"hit":"miss",(lineInArray+hitLatency));

	return(lineInArray + hitLatency);
}

void CacheClass::set_nextLevel(CacheClass* nLevel){
	nextLevel = nLevel;
}

int CacheClass::FindFreeMHSR(cycle_t curCycle)
{
	int i;
	CacheLineClass* line;
	bool hit;
	reg_t oldAddr;

	for (i=0; i<numMHSR; i++) {
		if (!mhsr[i].busy) {
    //if (mhsr[i].resolved ==-1){
			return(i);
		}
		if (mhsr[i].resolved < curCycle) {
			// MHSR is finished.  Free it.
			line = array.lookup(mhsr[i].lineAddress, NULL, &hit, &oldAddr, false);
			if (hit) {
				if (line->mhsr == i) {
					line->mhsr = -1;
				}
			}
			mhsr[i].resolved = 0;
			mhsr[i].busy = false;
			return(i);
		}
	}

	// No MHSRs available.
	return(-1);
}

int CacheClass::FindNextPort(cycle_t curCycle, cycle_t* portAvail)
{
	int i;
	int soonestPort;
	cycle_t soonestCycle;

	soonestPort = 0;
	soonestCycle = missPortAvail[0];

	for (i=1; i < numMissSrvPorts; i++) {
		if (soonestCycle > missPortAvail[i]) {
			soonestCycle = missPortAvail[i];
			soonestPort = i;
		}
	}

	*portAvail = soonestCycle;
	return(soonestPort);
}


bool CacheClass::Probe(unsigned int Tid, //RBRC
                       cycle_t curCycle,
                       reg_t addr1,
                       unsigned int length) {
	reg_t addr2;
	reg_t lineAddr1, lineAddr2;
	bool hit1, hit2;

	assert(length > 0);
	addr2 = (addr1 + insn_size*(length - 1));

	lineAddr1 = addr1 >> lineSize;
	lineAddr2 = addr2 >> lineSize;

	Access(Tid,curCycle, addr1, false, &hit1);
	if (lineAddr2 != lineAddr1) {
		Access(Tid,curCycle, addr2, false, &hit2);
	}
	else {
		hit2 = hit1;
	}

	return(hit1 && hit2);
}


// ER 06/19/01
void CacheClass::set_lat(unsigned int hit_lat, unsigned int miss_lat) {
	hitLatency = hit_lat;
	missLatency = miss_lat;
}

