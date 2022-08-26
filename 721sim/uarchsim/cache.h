#ifndef CACHE_H
#define CACHE_H
#pragma interface
#include <cstdio>
#include <cassert>
#include "common.h"
#include "decode.h"


#define	INVALID		-1


///////////////////////
// STANDARD CACHE
///////////////////////
template<class T>
class cache {
private:
	// The cache is a 2-D array of entries, each entry
	// is a tag + lru + pointer to object of type T.

	typedef
	struct {
		reg_t tag;
		unsigned int lru;
		T* contents;
	} entry;

	entry**	C;


public:
	// size = number of entries deep
	// assoc = number of associativity classes (ways)
	unsigned int size;
	unsigned int assoc;

	// stats
	unsigned int num_misses;

	// constructor
	cache(unsigned int size, unsigned int assoc) {
		unsigned int i,j;

		// First ensure that 'size' is a power of 2.
		assert( IsPow2(size) );

		C = new entry *[size];
		for (i = 0; i < size; i++) {
			C[i] = new entry[assoc];
			for (j = 0; j < assoc; j++) {
				C[i][j].tag = INVALID;
				C[i][j].lru = j;
				C[i][j].contents = (T*)NULL;
			}
		}

		this->size = size;
		this->assoc = assoc;
		this->num_misses = 0;
	}

	// destructor
	~cache() {
		//unsigned int i;
		//for (i = 0; i < size; i++)
		//   delete C[i];
		//delete C;
	}

	//
	// Flush cache
	//
	// Added by Quinn Jacobson, October 7, 1998.
	//
	void flush() {
		unsigned int i,j;

		for (i = 0; i < size; i++) {
			for (j = 0; j < assoc; j++) {
				C[i][j].tag = INVALID;
				C[i][j].lru = j;
				C[i][j].contents = (T*)NULL;
			}
		}
	}


	// Cache lookup and maintenance.
	// Inputs:
	//   (1) object id
	//   (2) pointer to object's contents
	//   (3) replace the entry on a cache miss
	// Outputs:
	//   (1) hit
	//   (2) old object id (i.e. id that was replaced, if miss)
	//   (3) return value: pointer to old object's contents
	T* lookup(reg_t id, T* contents,
	          bool* hit, reg_t* old_id,
	          bool replace,
	          bool use_raw_index = false,
	          unsigned int raw_index = 0);
};


template<class T>
T* cache<T>::lookup(reg_t id, T* contents,
                    bool* hit, reg_t* old_id,
                    bool replace,
                    bool use_raw_index, unsigned int raw_index) {
	unsigned int index;
	entry* set;
	unsigned int i;
	bool found;
	unsigned int hit_way;
	int replace_way;
	T* old_contents;

	index = MOD((use_raw_index ? raw_index : id), size);
	set = C[index];

	i = 0;
	found = false;
	while (i < assoc && !found) {
		if (set[i].tag == id) {
			found = true;
		}
		else {
			i += 1;
		}
	}

	if (found) {
		hit_way = i;

		// Update LRU state.
		for (i = 0; i < assoc; i++) {
			if (set[i].lru < set[hit_way].lru) {
				set[i].lru += 1;
			}
		}
		set[hit_way].lru = 0;

		// Set outputs of function.
		*hit = true;
		*old_id = set[hit_way].tag;
		old_contents = set[hit_way].contents;
	}
	else {
		// record the miss
		num_misses += 1;

		// Find replacement entry, and update LRU state.
		replace_way = -1;
		if (replace) {
			for (i = 0; i < assoc; i++) {
				if (set[i].lru == (assoc-1)) {	// least-recently used
					replace_way = (int)i;
					set[i].lru = 0;
				}
				else {
					set[i].lru += 1;
				}
			}
		}
		else {
			for (i = 0; i < assoc; i++) {
				if (set[i].lru == (assoc-1)) {	// least-recently used
					replace_way = (int)i;
				}
			}
		}
		assert(replace_way != -1);

		// Set outputs of function.
		*hit = false;
		*old_id = set[replace_way].tag;
		old_contents = set[replace_way].contents;

		// Perform the actual replacement.
		if (replace) {
			set[replace_way].tag = id;
			set[replace_way].contents = contents;
		}
	}

	return(old_contents);
}


#endif //CACHE_H
