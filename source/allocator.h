/* File: allocator.h
 * -----------------
 * Interface file for the custom heap allocator.
 */
#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <unistd.h> // for size_t

/* Function: myinit
 * ---------------
 * This must be called by a client before making any allocation
 * requests.  The function should return 0 if initiaization was success
 * and non-zero otherwise. The myinit function is also called to reset
 * the heap to an empty state. Our test harness calls this before starting
 * each new script when running against a whole suite of scripts.
 */
int myinit();

/* FUnction: mymalloc
 * ------------------
 * Custom version of malloc.
 */
void *mymalloc(size_t size);

/* Function: myrealloc
 * -------------------
 * Custom version of realloc.
 */
void *myrealloc(void *ptr, size_t size);

/* Function: myfree
 * ----------------
 * Custom version of free.
 */
void myfree(void *ptr);

#endif
