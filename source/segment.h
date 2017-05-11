/* File: segment.h
 * ---------------
 * A simple interface to a low-level memory allocator. These functions
 * allocate/extend a large segment of memory. Your allocator will call these
 * functions to manage the segment which is parceled out in response to
 * malloc requests. The segment is allocated in chunks of "page size" (4K 
 * on Linux).  There is a fixed upper bound (500MB) on the total segment size, 
 * beyond which the segment cannot be extended. If you attempt to extend the 
 * segment further, it will return NULL. 
 */
 
#ifndef _SEGMENT_H_
#define _SEGMENT_H_
#include <unistd.h> // for size_t

/* Function: InitHeapSegment
 * -------------------------
 * This function is called to initialize the heap segment and allocate the
 * segment to hold numPages, where each page of size HeapPageSize() bytes. 
 * The parameter numPages can be 0, in which case the heap segment is 
 * configured with no pages yet allocated.  The ExtendHeapSegment function
 * can be used to grow the size of the segement once initialized.
 * If InitHeapSegment is called a subsequent time, it will wipe out any
 * the current heap segment and start over with the given number of pages.
 * The InitHeapSegment function returns the base address of the heap 
 * segment on success, or NULL if the initialization failed. The base
 * address is always page-aligned.
 */
void *InitHeapSegment(int numPages);   
            
			
			
/* Function: ExtendHeapSegment
 * ---------------------------
 * This function is called to extend the size of the existing heap segment. The
 * segment must have beeen previously initialized using InitHeapSegment.
 * If extend is successful, the existing heap segment is grown to include an
 * additional "numPagesToAdd" pages and the starting address of those new pages is
 * returned (this address was previously the end of the heap segment). If the heap 
 * cannot be extended, NULL is returned. The address returned is always page-aligned.
 */
void *ExtendHeapSegment(int numPagesToAdd);


/* Functions: HeapSegmentStart, HeapSegmentSize
 * --------------------------------------------
 * HeapSegmentStart returns the pointer to the base address of the
 * heap segment. HeapSegmentSize returns the current size in bytes.
 * (The segment size will always be a multiple of HeapPageSize()).
 * Together, they identify the current extent of the heap segment.
 */
void *HeapSegmentStart();
size_t HeapSegmentSize();


/* Functions: HeapPageSize
 * -----------------------
 * HeapPageSize returns the number of bytes in a memory page.
 */
size_t HeapPageSize();

#endif
