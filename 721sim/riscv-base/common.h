// See LICENSE for license details.

#ifndef _RISCV_COMMON_H
#define _RISCV_COMMON_H

#include "config.h"

extern bool logging_on;

#define   likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

/* make the MOD function fast (in place of %): takes advantage of
   power of 2 mod operation... */
/* NOTE: "b" must be a power of 2 !!!! */
#define MOD(a,b)        ((a) & (b-1))

/* Slower MOD function that does not require power of 2. */
/* NOTE: Assumes, however, that "a < 2*b". */
#define MOD_S(a,b)        ( (a)>=(b) ? (a)-(b) : (a) )

/* Complements of multiscalar simulator... */
#define IsPow2(a)               (((a) & ((a) - 1)) ? 0 : 1)

// INFO macro definition.
#define INFO(fmt, args...)	\
	(fprintf(stderr, fmt, ##args),	\
	 fprintf(stderr, "\n"))

// INFO macro definition.
#ifdef RISCV_MICRO_DEBUG
  #define LOG(file,cycle,seq,pc,fmt,args...){ \
      if(logging_on){ \
        fprintf( file, "Cycle %" PRIcycle ": Seq %" PRIu64 " PC 0x%016" PRIx64 " ", cycle,seq,pc);  \
        fprintf( file, fmt, ##args);	\
  	    fprintf( file, "\n"); \
      } \
    }

  // ifprintf macro definition.
  #define ifprintf(condition, file, args...){	\
      if(condition){  \
       fprintf(file, ##args);	\
      } \
    }

#else
  #define LOG(file,cycle,seq,pc,fmt,args...){ }
  #define ifprintf(condition, file, args...){	}
#endif


#define NO_FREE_BIT		(0xDEADBEEF)
#define BIT_IS_ZERO(x,i)	(((x) & (((unsigned long long)1) << i)) == 0)
#define BIT_IS_ONE(x,i)		(((x) & (((unsigned long long)1) << i)) != 0)
#define SET_BIT(x,i)		(x |= (((unsigned long long)1) << i))
#define CLEAR_BIT(x,i)		(x &= ~(((unsigned long long)1) << i))

#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif



/* various useful macros */
#ifndef MAX
#define MAX(a, b)    (((a) < (b)) ? (b) : (a))
#endif
#ifndef MIN
#define MIN(a, b)    (((a) < (b)) ? (a) : (b))
#endif

/* for printing out "long long" vars */
#define LLHIGH(L)		((long)(((L)>>32) & 0xffffffff))
#define LLLOW(L)		((long)((L) & 0xffffffff))

/* bind together two symbols, at proprocess time */
#define SYMCAT(X,Y)	X##Y

/* size of an array, in elements */
#define N_ELT(ARR)   (sizeof(ARR)/sizeof((ARR)[0]))

/* rounding macros, assumes ALIGN is a power of two */
#define ROUND_UP(N,ALIGN)	(((N) + ((ALIGN)-1)) & ~((ALIGN)-1))
#define ROUND_DOWN(N,ALIGN)	((N) & ~((ALIGN)-1))


#endif
