NAME(s)
<Be sure to include real name & sunet username for you and partner (if any)>

Cecil Woebker - cwoebker
Benjamin Yep - benny3t3

--------------------------------------------------------------------------------

DESIGN


Overview:

    We used headers and footers as housekeeping.
    Footers are needed as boundary Tags. (for coalesce)
    We use a first-fit function to find free blocks in our free lists.
    We use coalescing to merge free blocks.
    We implemented multiple explicit free lists, that are doubly linked.
    Links are stored in the second 4 bytes of headers and footers.
    Footers for links to next free blocks.
    Headers for links to previous free blocks.


Whether/how to store block housekeeping data (block headers, footers, not co-located with blocks at all?)

    - We use headers and footers
        - footers are needed as boundary tags
    - 8 bytes each
    - first 4 bytes for size and Allocated bit
    - second for for a pointer to the free block:
        - next block if its a footer
        - previous block if its a header

Deciding when/how to split and coalesce blocks

    - we coalesce every time we free
    - we coalesce every time we extend the heap to make sure free blocks at
    the end get merged with the new block
    - we split every time we place into a free block or into a freshly extended part of the heap
        - which is when we find a big enough free block to use
        - when we extend the heap


Determining how to manage the free list (implicit? explicit? sorted? in a tree? segregated by size?)

    - doubly linked explicit free list
    - pointers are stored in the second 4 bytes of the header struct
    - footer for next element
    - header for previous element
    - not segregated by size

Finding available space using first-fit, next-fit, best-fit or something else entirely

    - we use first fit, since best fit would penalize performance too much

Are the blocks themselves segregated by size in the storage?

    - Yes we have exact bins for 24 through 64 for every multiple of 8
    - from 64 to 8192 we have range bins form one power of two to the next
    - a total of 14 bins

Do they have buddies?

    - No, we did not implement a buddy system.

Will you use the same strategy for all sizes of request, or have different handling by size?

    - We use the same strategy for all sizes.

Will you implement a standalone realloc or just delegate to malloc/memcpy/free?

    - We delegate to malloc/memcpy/free
    - But we have some extra cases that improve utilization and throughput,
        when applicable:
        - shrinking just returns the old pointer
        - Extending by using the next block if its free
        - we always extend by a minimum amount so we don't
            have to repeatedly call realloc on small values
        - if the block is already big enough we just return

--------------------------------------------------------------------------------

RATIONALE

Headers, Footers and the doubly linked explicit free list allowed for
a very structured setup that could be easily worked on.
Boundary Tags (Footers) are used for quick coalescing.

first-fit searching for free blocks was used due to fast performance
unlike best-fit - sacrifices utilization for throughput

We have multiple free lists segregated by size, since first-fit can use that
to find free blocks more quickly.

Buddies and handling different sizes different ways, were not done
since that would have added a lot of extra code.

--------------------------------------------------------------------------------

OPTIMIZATION

We used Valgrind's callgrind tool. To see what our biggest issues are.
We tried to reduce repeated function calls as much as possible.
Additionally we tried to simplify these repeated calls as much as possible.

Initially we stored void pointers to the payloads in the free list but we
changed that to headers realizing, that we made heavy use 
of HeaderForPayload
We went ahead and made that change in many other functions too, 
to increase performance

For nested if statements, we looked at what was called the most frequent,
So that these cases could be checked first.

--------------------------------------------------------------------------------

EVALUATION

We saw that utilization drops low, if many malloc requests are made and than
all those are freed.
Otherwise our results are pretty similar.
Internal fragmentation if a lot of small mallocs are requested since we have
16 bytes of additional data
Realloc-pattern has bad utilization, potentially because we don't backward realloc.

