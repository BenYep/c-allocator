/* File: segment.c
 * ---------------
 * Handles low-level storage underneath the dynamic allocator. It reserves
 * the large memory segment using the OS-level mmap facility and then
 * opens it up on demand based on calls to ExtendHeapSegment.
 */
 
#include "segment.h"
#include <sys/mman.h>

#define MB (1 << 20)
#define MAX_HEAP (500*(MB))

// static variables track state of heap segment
static void *segment_start = NULL;  // base of heap segment
static int segment_size = 0; // expressed in bytes
static size_t page_size;

// Return address of the first heap byte
void *HeapSegmentStart()
{
    return segment_start;
}

// Returns the in-use size of heap (in bytes)
size_t HeapSegmentSize() 
{
    return segment_size;
}

// Returns the number of bytes in each page
size_t HeapPageSize() 
{
    if (page_size == 0)
        page_size = getpagesize();
    return page_size;
}


// Discards any previous segment by unmapping old segment
// Then reinits segments by reserving new segment with mmap
void *InitHeapSegment(int numPages)
{
    if (segment_start != NULL) { // discard any existing segment
        if (munmap(segment_start, MAX_HEAP) == -1) return NULL;
        segment_start = NULL;
    }
    // reserve entire segment in advance
    if ((segment_start = mmap(0, MAX_HEAP, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
        return NULL;
    segment_size = 0;
    return ExtendHeapSegment(numPages);
}


// Extends the segment by number of pages and returns the start address of new pages
// In this model, the segment cannot shrink
void *ExtendHeapSegment(int numPagesToAdd)
{	
    if (segment_start == NULL) return NULL; // Init has not been called?

    void *previous_end = (char *)segment_start + segment_size;
    
    if (numPagesToAdd <= 0) return previous_end;
    size_t increment = numPagesToAdd*HeapPageSize();
    if (increment > MAX_HEAP || (segment_size + increment) > MAX_HEAP) 
        return NULL;
    segment_size += increment;
    if (mprotect(previous_end, increment, PROT_READ|PROT_WRITE) == -1)
        return NULL;
    return previous_end;
}

