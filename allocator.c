/*
 * File: allocator.c
 * ------------------
 * A trivial allocator. Very simple code, heinously inefficient. 
 * An allocation request is serviced by incrementing the heap segment
 * to place new block on its own page(s). The block has a pre-node header
 * containing size information. Free is a no-op: blocks are never coalesced
 * or reused. Realloc is implemented using malloc/memcpy/free.
 * Using page-per-node means low utilization. Because it grows the heap segment
 * (using expensive OS call) for each node, it also has low throughput
 * and terrible cache performance.  The code is not robust in terms of
 * handling unusual cases (e.g. segment exhausted or realloc smaller).
 *
 * In short, this implementation has not much going for it other than being
 * the smallest amount of code that "works" for ordinary cases.  But
 * even that's still a good place to start from.
 */

#include <stdio.h>

#include <errno.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "allocator.h"
#include "segment.h"

// Heap blocks are required to be aligned to 8-byte boundary
#define ALIGNMENT 8
#define HEADER2 16

//Other Basic Constants
#define HEAP_EXTEND_SIZE (2 << 14) // 32768

#define MIN_REALLOC (2 << 8) // 512

#define MINIMUM_BLOCK_SIZE 3 * ALIGNMENT

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define NUMBINS 14

typedef struct headerT {
   size_t payloadSize;
   struct headerT *ptr; // unused field to make struct 8 bytes, simplifies alignment
} headerT;

typedef struct freeList {
    headerT *first;
    headerT *last;
    int size;
} freeList;

void *endHeapSegment;
void *startHeapSegment;
size_t pageSize;

freeList mainList;

freeList bins[NUMBINS]; // exact bins till 64 and the rest a range of power two, until 8192


static inline int numForSize(unsigned int size) {
    /*if (size <= 24) return 0;
    if (size <= 64) { // 1 2 3 4 5 for exact bin
        size -= 32;
        size /= 8;
        return size + 1;
    }
    if (size <= 8192) {
        for (int i = 7; i <= 13; i++) {
            if (size <= (1<<i)) {
                size = i - 7;
                break;
            }
        }
        return size + 6;
    }
    return NUMBINS-1;*/
    if (size <= 64) {
        switch (size) {
            case 24:
                return 0;
            case 32:
                return 1;
            case 40:
                return 2;
            case 48:
                return 3;
            case 56:
                return 4;
            case 64:
                return 5;
        }
    } else if (size <= 8192) {
        for (int i = 7; i <= 13; i++) {
            if (size <= (1<<i)) {
                return (i - 7) + 6;
            }
        }
    }
    return NUMBINS-1;
}

// Rounds up size to nearest multiple of given power of 2
// does this by adding mult-1 to sz, then masking off the
// the bottom bits, result is power of mult
// NOTE: mult has to be power of 2 for this to work!
static inline size_t RoundUp(size_t sz, int mult)
{
   return (sz + mult-1) & ~(mult-1);
}

static inline size_t createHeaderSize(size_t sizeBits, unsigned int allocationBit)
{
    return ((sizeBits) | (allocationBit));
}

static inline size_t getSize(headerT *header)
{
    return ((unsigned int)(header->payloadSize) & ~0x7);
}

static inline size_t isAllocated(headerT *header)
{
    return ((unsigned int)(header->payloadSize) & 0x1);
}

////////////////////////////
// BLOCK HELPER FUNCTIONS //
////////////////////////////

// Given a pointer to block header, advance past
// header to access start of payload
static inline void *PayloadForHeader(headerT *header)
{
   return (char *)header + sizeof(headerT);
}

// Given a pointer to start of payload, simply back up
// to access its block header
static inline headerT *HeaderForPayload(void *payload)
{
    return (headerT *)((char *)payload - sizeof(headerT));
}

// Warning these two functions depend on the header!!!!!!! so make sure to reget them or sth...

static inline headerT *FooterForPayload(void *payload)
{
    return (headerT *)((char *)payload + (getSize(HeaderForPayload(payload)) - HEADER2));
}

static inline headerT *FooterForHeader(headerT *header){
    //return FooterForPayload(PayloadForHeader(header));
    return (headerT *)((char *)header + getSize(header) - sizeof(headerT));
}

// getting next or previous Header
static inline headerT *nextHeader(headerT *header)
{
    size_t currentSize = getSize(header);
    if( (void *)((char *)header+currentSize) == endHeapSegment)
        return NULL; // last Header reached
    return (headerT *)((char *)header+currentSize);
}

static inline headerT *previousHeader(headerT *header)
{
    if((void *)header == startHeapSegment){
        return NULL; // payload is the first Header , nothing there before
    }
    return (headerT *)((char *)(header) - getSize( (headerT *) ((char *)header - sizeof(headerT)) ));
}

/*void validate_heap() {
    void *end = (void *)((char *)HeapSegmentStart() + HeapSegmentSize());
    for(headerT *header = (headerT *)HeapSegmentStart(); (unsigned int)header < (unsigned int)end; header = (headerT *)((char *)header + getSize(header))) {
        if (isAllocated(header) == 0) { // if free check if in free list
            bool found = false;
            void *ptr = PayloadForHeader(header);
            void *currentBlock = mainList.first;
            while (currentBlock != NULL) {
                if (ptr == currentBlock) {
                    found = true;
                    break;
                }
                currentBlock = FooterForPayload(currentBlock)->ptr;
            }
            if (!found) {
                fprintf(stderr, "Error free block not in free list. %p\n", ptr);
                exit(1);
            }
        }
    }
    void *currentBlock = mainList.first;
    while (currentBlock != NULL) {
        if (isAllocated(HeaderForPayload(currentBlock)) == 0) {
            currentBlock = FooterForPayload(currentBlock)->ptr;
        } else {
            fprintf(stderr, "Error non free block in free list. %p\n", currentBlock);
            exit(2);
        }
    }
}
*/

// as first element
static void addToFreeList(headerT *freeHeader) {
    int index = numForSize(getSize(freeHeader));
    if (bins[index].first == NULL || bins[index].last == NULL) { // no elements so far
        FooterForHeader(freeHeader)->ptr = NULL; // set next to NULL
        freeHeader->ptr = NULL; // set previous of first element to NULL
        bins[index].first = freeHeader;
        bins[index].last = freeHeader;
    } else { // append to list as first element
        FooterForHeader(freeHeader)->ptr = bins[index].first;
        freeHeader->ptr = NULL;
        bins[index].first->ptr = freeHeader;
        bins[index].first = freeHeader; // set new last ptr
        //printf("Added[%p]: -> [%p] -> [%p]\n", bins[index].first, HeaderForPayload(bins[index].last)->ptr, bins[index].last);
    }
    bins[index].size++;
}

static void removeFromFreeList(headerT *freeHeader) {
    int index = numForSize(getSize(freeHeader));
    headerT *prev_Header = freeHeader->ptr;
    headerT *next_Header = FooterForHeader(freeHeader)->ptr;

    if (prev_Header) {
        if (next_Header) {
            // just remove and reconnect
            FooterForHeader(prev_Header)->ptr = next_Header;
            next_Header->ptr = prev_Header;
        }
        else {
            // next is NULL, so prev is new last element
            FooterForHeader(prev_Header)->ptr = NULL;
            bins[index].last = prev_Header;
        }
    }
    else {
        if (next_Header) {
            // prev is NULL, so next is new first element
            next_Header->ptr = NULL;
            bins[index].first = next_Header;
        }
        else {
            // both are invalid, so list is empty
            bins[index].first = NULL;
            bins[index].last = NULL;
        }
    }

    bins[index].size--;
}


/* find the next fit in the free list
 */
static headerT *find_fit(size_t adjustedSize)
{
    int index = numForSize(adjustedSize);

    for (int i = index; i < NUMBINS; i++) {
        if (bins[i].first == NULL || bins[i].last == NULL) {
            continue;; // list is empty, nothing to find
        }
        headerT *currentHeader = bins[i].first;
        while (currentHeader != NULL) {
            if (getSize(currentHeader) >= adjustedSize) {
                return currentHeader;
            }
            currentHeader = FooterForHeader(currentHeader)->ptr;
        }
    }
    return NULL;

    /* find_next code thats broken
    if (curFreePos == NULL) {
        curFreePos = bins[index].first;
    }
    int count = 0;
    while (true) {
        if (curFreePos == NULL) {
            curFreePos = bins[index].first; // i am at the end
            continue;
        }
        void *ret = NULL;
        if (getSize(HeaderForPayload(curFreePos)) >= adjustedSize) {
            ret = curFreePos;
        }
        curFreePos = FooterForPayload(curFreePos)->ptr;
        if (ret) return ret;
        if (count == bins[index].size) {
            break;
        }
        count++;
    }
    return NULL;*/
}

/* The responsibility of the myinit function is to set up the heap to
 * its initial, empty ready-to-go state.  This will be called before
 * any allocation requests are made.  The myinit function may also
 * be called later in program and is expected to wipe out the current
 * heap contents and start over fresh. This "reset" option is specificcally
 * needed by the test harness to run a sequence of scripts, one after another, 
 * without restarting program from scratch.
 */
int myinit()
{
    InitHeapSegment(0); // reset heap segment to empty, no pages allocated

    mainList.first = NULL;
    mainList.last = NULL;
    mainList.size = 0;

    for (int i = 0; i < NUMBINS; i++) {
        bins[i].first = NULL;
        bins[i].last = NULL;
        bins[i].size = 0;
    }

    startHeapSegment = HeapSegmentStart();
    endHeapSegment = (void*)((char*)startHeapSegment+(unsigned int)HeapSegmentSize());
    pageSize = HeapPageSize();

    return 0;
}

static headerT *coalesce(headerT *header)
{
    bool previousBlockAllocated=0;
    bool nextBlockAllocated=0;

    headerT *prev_Header = previousHeader(header);
    headerT *next_Header = nextHeader(header);

    if((prev_Header==NULL)||(isAllocated(prev_Header)==1)){
        previousBlockAllocated=true;
    } else {
        removeFromFreeList(prev_Header);
    }
    if((next_Header==NULL)||(isAllocated(next_Header)==1)){
       nextBlockAllocated=true;
    } else {
        removeFromFreeList(next_Header);
    }

    size_t size = getSize(header);
    size_t newSize;

    if (previousBlockAllocated && !nextBlockAllocated) { // next is free
        size += getSize(next_Header);
        newSize = createHeaderSize(size, 0);
        header->payloadSize = newSize;
        FooterForHeader(header)->payloadSize = newSize;
    }
    else if (!previousBlockAllocated && nextBlockAllocated) { // previous is free
        size += getSize(prev_Header);
        newSize = createHeaderSize(size, 0);
        prev_Header->payloadSize = newSize;
        FooterForHeader(header)->payloadSize = newSize;

        header = prev_Header;
    }
    else if (!previousBlockAllocated && !nextBlockAllocated) { // both are free
        size += getSize(next_Header) +
                getSize(prev_Header);
        newSize = createHeaderSize(size, 0);
        prev_Header->payloadSize = newSize;
        FooterForHeader(next_Header)->payloadSize = newSize;

        header = prev_Header;
    }
    else { // none are free
        // just return
        return header;
    }
    return header;
}

static void place(headerT *header, int adjustedSize)
{
    // we can definetely remove the old pointer form the free list
    removeFromFreeList(header);

    headerT *footer= FooterForHeader(header);
    size_t remainder_t = getSize(header) - adjustedSize;

    //printf("Allocated bytes: %d = %d - %d\n", remainder_t, getSize(header), adjustedSize);

    if (remainder_t >= MINIMUM_BLOCK_SIZE) {
        //remainder bigger than min_blocksize, so we should split
	    header->payloadSize = createHeaderSize(adjustedSize, 1);
	    FooterForHeader(header)->payloadSize = header->payloadSize;

        headerT *split = nextHeader(header);
        split->payloadSize = createHeaderSize(remainder_t, 0);
        FooterForHeader(split)->payloadSize = createHeaderSize(remainder_t, 0);
        addToFreeList(split);
    }
    else{
        // remainder not bigger so we should take the whole memory
        // so all we do is that we set the allocated bit
		header->payloadSize = createHeaderSize(getSize(header),1);
		footer->payloadSize = header->payloadSize;
	}
}


/*
Check the list of free nodes to see if there are nodes that can be supplied to the client that have
already been allocated and freed.
If there are none, then bring in a new page of memory and provide the client with the newly allocated data
*/
void *mymalloc(size_t requestedSize)
{
    if (requestedSize == 0) {
      return NULL;
    }
    // calcluate adjusted size
    size_t adjustedSize;
    if (requestedSize<=ALIGNMENT) {
        adjustedSize = MINIMUM_BLOCK_SIZE; // 24 bytes
    } else {
        adjustedSize = 2 * sizeof(headerT) + RoundUp(requestedSize, 8);
    }

    // make sure second argument is a multiple of two......
    headerT *header;

    if ((header = find_fit(adjustedSize)) != NULL) { // if NULL nothing was found
        //place and split
        place(header, adjustedSize);
        return PayloadForHeader(header);
    }

    // no fit found, extend memory and place block */
    size_t extendsize = MAX(adjustedSize, HEAP_EXTEND_SIZE);

    size_t totalSize = RoundUp(extendsize, pageSize);
    size_t npages = totalSize/pageSize;
    //headerT *header;
    if ((header = ExtendHeapSegment(npages)) == NULL) {
      errno = ENOMEM; // heap segment cannot be extended
      return NULL;
    }
    endHeapSegment = (void*)((char*)startHeapSegment+(unsigned int)HeapSegmentSize());
    header->payloadSize = createHeaderSize(totalSize, 0);
    FooterForHeader(header)->payloadSize = header->payloadSize;

    //ptr = PayloadForHeader(header);
    header = coalesce(header);
    addToFreeList(header);

    place(header, adjustedSize);
    return PayloadForHeader(header);
}


void myfree(void *ptr)
{
    if (ptr == NULL) return;

    headerT *header = HeaderForPayload(ptr);
    if (isAllocated(header)==0) return;

    size_t size = getSize(header);
    header->payloadSize = createHeaderSize(size, 0);
    FooterForPayload(ptr)->payloadSize = createHeaderSize(size, 0);

    //printFreeList();

    header = coalesce(header);
    addToFreeList(header);

    //printFreeList();
    //validate_heap();
}



// realloc built on malloc/memcpy/free is easy to write.
// This code will work ok on ordinary cases, but needs attention
// to robustness. Realloc efficiency can be improved by
// implementing a standalone realloc as opposed to
// delegating to malloc/free.
void *myrealloc(void *oldptr, size_t requestedSize)
{
    // check docs on requirements
    if (oldptr == NULL) {
        return mymalloc(requestedSize);
    }
    headerT *oldHeader = HeaderForPayload(oldptr);
    if (isAllocated(oldHeader) == 0) {
        return NULL; // not allocated so no need to realloc - need to check for errno to set
    }
    if (requestedSize == 0) {
        myfree(oldptr);
        return NULL;
    }

    // thinking of anticipating reallocation - repeats usually

	//payload size of old pointer
	int oldPayloadSize = getSize(oldHeader);

	int adjustedSize;
	if(requestedSize<=ALIGNMENT){
		adjustedSize = MINIMUM_BLOCK_SIZE;
	}else{
		adjustedSize = HEADER2 + RoundUp(requestedSize, 8);
	}
    //smaller than before, or the same
    if (adjustedSize <= oldPayloadSize) {
	   //if there is enough room at the end of this smaller block to add free nodes, do so.
    	/*if(oldPayloadSize-adjustedSize >= MINIMUM_BLOCK_SIZE){
            size_t oldSize = createHeaderSize(adjustedSize,1);
    		oldHeader->payloadSize = oldSize;
    		FooterForHeader(oldHeader)->payloadSize = oldSize;

    		headerT *split = nextHeader(oldHeader);

    		size_t remainder_t = oldPayloadSize - adjustedSize;
            size_t nextSize = createHeaderSize(remainder_t,0);
    		split->payloadSize = nextSize;
    		split->payloadSize = nextSize;
    		split = coalesce(split);
    		addToFreeList(split);

    		return oldptr;
    	} else { // just take the old one since we can't shrink it
            return oldptr;
        }*/
        return oldptr; // return the old pointer all the time, fast!!!
    } else { // new size is bigger
        if (adjustedSize - oldPayloadSize < MINIMUM_BLOCK_SIZE) {
            // standard, no free block can be found -> don't even try
        } else {
            headerT *next_Header = nextHeader(oldHeader);
            // if exists and free
            if (next_Header != NULL && isAllocated(next_Header) == 0) {
                size_t newSize = oldPayloadSize + getSize(next_Header);
                // enough space there
                if (newSize >= adjustedSize) {
                    removeFromFreeList(next_Header);
                    newSize = createHeaderSize(newSize, 1);
                    oldHeader->payloadSize = newSize;
                    FooterForHeader(oldHeader)->payloadSize = newSize;
                    return oldptr;
                }
            }
        }
    }

    size_t newSize;
    if ((adjustedSize - oldPayloadSize) < MIN_REALLOC) newSize = (oldPayloadSize + MIN_REALLOC);
    else newSize = requestedSize;
    // standard case
    void *newptr = mymalloc(newSize);
    memcpy(newptr, oldptr, oldPayloadSize < newSize ? oldPayloadSize: newSize);
    myfree(oldptr);
    return newptr;
}


