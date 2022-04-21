/*
  Filename   : mm.c
  Author     : Drew Walizer
  Course     : CSCI 380-01
  Assignment : Malloc1
  Description: Implementation of Implicit Memory Allocations 
    by creating my own malloc, free, and realloc functions. 
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "memlib.h"
#include "mm.h"

/****************************************************************/
// Useful type aliases

typedef uint64_t word;
typedef uint32_t tag;
typedef uint8_t  byte;
typedef byte*    address; 

/****************************************************************/
// Useful constants

const uint8_t WORD_SIZE = sizeof (word);
const uint8_t DWORD_SIZE = sizeof(word) * 2;
// Add others... 

address g_heapBase;
/****************************************************************/
// Inline functions

/* returns header address given basePtr */
static inline tag* 
header (address p)
{
  return (tag *) p - 1;
}

/* retrun true IFF block is allocated */
static inline bool
isAllocated (address p)
{
  return *header(p) & 1;
}

/* returns size of block (words) */
static inline uint32_t
sizeOf (address p)
{
  return *header(p) & (uint32_t) -2;
}

/* returns footer address given basePtr */
static inline tag*
footer (address p) 
{
  return (tag *) (p + (sizeOf(p) * WORD_SIZE)) - 2; 
}

/* gives the basePtr of next block */
static inline address
nextBlock (address p)
{
  return p + sizeOf(p) * WORD_SIZE;
}

/* returns a pointer to the prev block's 
   footer. HINT: you will use header() */
static inline tag*
prevFooter (address p)
{
  return (tag *) header(p) - 1;
}

/* returns a pointer to the next block's 
   header. HINT: you will use sizeOf() */
static inline tag* 
nextHeader (address p)
{
  return header(nextBlock(p));
}

/* gives the basePtr of prev block */
static inline address 
prevBlock (address p)
{
  return p - (*prevFooter(p) & (uint32_t) -2) * WORD_SIZE;
}

/* basePtr, size, allocated */ 
static inline void
makeBlock (address p, uint32_t len, bool allocated)
{
  *header(p) = len | allocated;
  *footer(p) = len; 
}

/* basePtr - toggels alloced/free */
static inline void
toggleBlock (address p)
{
  if (isAllocated(p))
    *header(p) = (sizeOf(p) >> 1) << 1;
  else 
    *header(p) = sizeOf(p) | 1;
}

/****************************************************************/
// Non-inline functions


// Gary M. Zoppetti
// Testing framework for the Malloc Lab

void
printPtrDiff (const char* header, void* p, void* base)
{
  printf ("%s: %td\n", header, (address) p - (address) base);
}

void
printBlock (address p)
{
  printf ("Block Addr %p; Size %u; Alloc %d\n",
	  p, sizeOf (p), isAllocated (p)); 
}

void 
mm_check ()
{
  printf ("\nBlocks\n");
  for (address p = g_heapBase; sizeOf(p) != 0; p = nextBlock(p))
  {
    printBlock(p);
    printPtrDiff ("header", header (p), mem_heap_lo());
    printPtrDiff ("footer", footer (p), mem_heap_lo());
    printPtrDiff ("nextHeader", nextHeader (p), mem_heap_lo());
    printPtrDiff ("prevFooter", prevFooter (p), mem_heap_lo());
    printPtrDiff ("prevBlock", prevBlock (p), mem_heap_lo());
    printPtrDiff ("nextBlock", nextBlock (p), mem_heap_lo());
  }
}

// int
// main ()
// {
//   mem_init();
//   mm_init();
//   // address first = (address) mm_malloc(22);
//   // // mm_check();
//   // // mm_free(first);
//   // (address) mm_malloc(26);
//   // // mm_check();  
//   // (address) mm_realloc(first, 4);
//   // mm_check();  
//   // mm_free(first); 
//   // mm_free(third);
//   // mm_free(second);
//   mm_check();
  
//   return 0;
// }

/*
  numWords - The number of words to extend the heap by. 

  Extends the heap space by numWords or numWords * WORD_SIZE bytes.
  Then creates a free block at the end of the numWords long and 
  resets the end setinel header. 
*/
static inline address
extendHeap (uint32_t numWords)
{
  // Use mem_sbrk to increase the heap size by the appropriate #
  //   of bytes. Return NULL on error.
  address extendPtr;
  extendPtr = mem_sbrk((int) numWords * WORD_SIZE);
  if (extendPtr == (void *) -1)
    return NULL;

  // Make a free block of size "numWords".
  makeBlock (extendPtr, numWords, 0);

  // Place the end sentinel header.
  *nextHeader (extendPtr) = 0 | 1;
  
  // initial size 
  if (mem_heapsize() == (6 * DWORD_SIZE))
  {
    return extendPtr;
  }
  // Coalesce newly created free block.
  else if (!isAllocated(prevBlock(extendPtr)))
  {
    // Combine blocks.
    address prevPtr = prevBlock(extendPtr);
    uint32_t newSize = sizeOf(prevPtr) + sizeOf(extendPtr);

    *header(prevPtr) = newSize;
    *footer(extendPtr) = newSize;
    extendPtr = prevPtr;
  }
  return extendPtr;
}

/*
  ptr - a pointer to a free block. 
  numWords - the number of words to allocate. 

  Called by mm_malloc when a valid free block found has a 
  size > then the malloc size. splitFreeBlock will split a free 
  block into an allocated block and free block returning an 
  address pointer to the allocated block. 
*/
address
splitFreeBlock (address ptr, uint32_t numWords)
{
  uint32_t freeSize = sizeOf(ptr) - numWords;
  *footer(ptr) = freeSize;
  *header(ptr) = numWords;
  *footer(ptr) = numWords;
  *nextHeader(ptr) = freeSize;
  toggleBlock(ptr);
  return ptr;
}

/***********************************************************************/


/*
  Allocating initial heap area and default-initializing any global variables
  Place sentinel blocks in heap, one in prologue and one in epilogue
  Return -1 if any problems, else 0
*/
int
mm_init (void)
{
  // Use mem_sbrk to allocate a heap of some # of DWORDS, perhaps 4
  //   like in the guide (at least 1 DWORD). 
  // Set g_heapBase to point to DWORD 1 (0-based). 
  address initPtr;
  initPtr = mem_sbrk(DWORD_SIZE);
  if (initPtr == (void *) -1)
    return -1;
  // 2 Words for 1 wasted word then the setinel footer / 1st block header for 16 byte alignment. 
  makeBlock (initPtr, 2, 0);
  // convert bytes to words. 
  uint32_t numWords = (((4 * DWORD_SIZE) + WORD_SIZE + (DWORD_SIZE - 1)) / DWORD_SIZE) * 2;
  g_heapBase = extendHeap(numWords);
  if (g_heapBase == NULL)
    return -1;
  // Place the begin sentinel footer.
  *prevFooter (g_heapBase) = 0 | 1;
  return 0;
}

/****************************************************************/

/*
  size - the amount of bytes to allocate. 

  Allocates a block of size bytes to the heap. 
  Returns pointer to newly allocated block payload. 
*/
void*
mm_malloc (uint32_t size)
{
  // convert bytes to words. 
  uint32_t numwords = ((size + WORD_SIZE + (DWORD_SIZE - 1)) / DWORD_SIZE) * 2;
  bool foundBlock = 0; 
  address payloadPtr;
  for (address p = g_heapBase; sizeOf(p) != 0; p = nextBlock(p))
  {
    // block must be free.
    if (!isAllocated(p))
    {
      // found block allocate it.
      if (sizeOf(p) == numwords)
      {
        foundBlock = 1;
        toggleBlock(p);
        payloadPtr = p;
        break;
      }
      // split free block and allocate it.
      else if (sizeOf(p) > numwords)
      {
        foundBlock = 1;
        payloadPtr = splitFreeBlock(p, numwords);
        break;
      }
    }
  }
  // no large enough free block in heap.
  if (!foundBlock)
  {
    address extendPtr = extendHeap(numwords);
    // if newly extended free block combined with previously ending free block.
    if (sizeOf(extendPtr) > numwords)
    {
      payloadPtr = splitFreeBlock(extendPtr, numwords);
    }
    else
    {
      payloadPtr = extendPtr;
      toggleBlock(extendPtr);
    }
  }
  return payloadPtr;
}

/****************************************************************/

/*
  ptr - pointer to a previously malloc'd or realloc'd block. 

  Frees a previously malloc'd or realloc'd block 
  and coalesces free blocks next to one another. 
*/
void
mm_free (void *ptr)
{
  if (isAllocated(ptr))
  {
    // blocks next to are allocated.
    if (isAllocated(prevBlock(ptr)) && isAllocated(nextBlock(ptr)))
    {
      toggleBlock(ptr);
    } 
    // prev block free, next block allocated.
    else if (isAllocated(prevBlock(ptr)) && !isAllocated(nextBlock(ptr)))
    {
      address nextPtr = nextBlock(ptr);
      uint32_t newSize = sizeOf(nextPtr) + sizeOf(ptr);
      *header(ptr) = newSize;
      *footer(nextPtr) = newSize;
    }
    // next block free, prev block allocated. 
    else if (!isAllocated(prevBlock(ptr)) && isAllocated(nextBlock(ptr)))
    {
      address prevPtr = prevBlock(ptr);
      uint32_t newSize = sizeOf(prevPtr) + sizeOf(ptr);
      *header(prevPtr) = newSize;
      *footer(ptr) = newSize;
      ptr = prevPtr;
    }
    // both blocks next to are free.
    else
    {
      address prevPtr = prevBlock(ptr);
      address nextPtr = nextBlock(ptr);
      uint32_t newSize = sizeOf(prevPtr) + sizeOf(ptr) + sizeOf(nextPtr);
      *header(prevPtr) = newSize;
      *footer(nextPtr) = newSize;
      ptr = prevPtr; 
    }   
  }
}

/****************************************************************/

/*
  ptr - pointer to a previously malloc'd or reallc'd block.
  size - number of bytes to reallc. 

  If ptr is NULL mm_realloc should allocate a block of size bytes. 
  If size is 0 mm_realloc should free ptr.
  If ptr is not NULL and size is not 0 then resize the block to 
  size bytes while preserving the contents of the old block upto 
  the minimum of old number of words and new number of words. 
  Returns an address pointer to the new block.
*/
void*
mm_realloc (void *ptr, uint32_t size)
{
  address *newptr = NULL;
  if (ptr == NULL)
    newptr = mm_malloc(size);
  else if (size == 0)
    mm_free(ptr);
  else 
  { 
    // convert bytes to words.
    uint32_t newsize = ((size + WORD_SIZE + (DWORD_SIZE - 1)) / DWORD_SIZE) * 2;
    uint32_t minsize = sizeOf(ptr);
    if (minsize == newsize)
      return ptr;
    else if (minsize > newsize)
      minsize = newsize;
    newptr = mm_malloc(size);
    newptr = memcpy(newptr, ptr, (size_t) minsize * WORD_SIZE);
    mm_free(ptr);
  }
  return newptr;
}
