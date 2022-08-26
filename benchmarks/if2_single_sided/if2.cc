#include <stdio.h>
#include <stdlib.h>
//#include <cstdlib>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>

#define ARRAY_SIZE 100000
//#define ARRAY_SIZE 1000

int main(int argc, char *argv[]) {
   uint64_t sum;
   int32_t array[ARRAY_SIZE];

   for (uint64_t i = 0; i < ARRAY_SIZE; i++)
      array[i] = rand();

#if 0
   for (uint64_t i = 0; i < ARRAY_SIZE; i++)
      printf("%d\n", array[i]);
#endif

   sum = 0;
   for (uint64_t i = 0; i < ARRAY_SIZE; i++) {
      if (array[i] > rand())
         sum += array[i];
   }

/*
 
Instead of:
   if (array[i]) sum++;
Do this:
   sum += array[i];

*/

   printf("sum = %lu\n", sum);

   return(0);
}
