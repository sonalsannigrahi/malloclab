/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Sonal Sannigrahi",
    /* First member's email address */
    "sonal.sannigrahi@polytechnique.edu",
    /* Second member's full name (leave blank if none) */
    "Noah Sarfati",
    /* Second member's email address (leave blank if none) */
    "noah.sarfati@polytechnique.edu"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */

#define WSIZE 4                /* Word and header/footer size (bytes) */
#define DSIZE 8                /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12)    /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 2 * DSIZE /* alignement +header + footer */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))
/*#define NEXT_FREE(bp) (*(char **)(bp))
#define PREV_FREE(bp)*/
/* Macros for explicit free list */
/* Read and write a word to successor free block of current free block*/
#define GET_PREV(bp) (*(unsigned int *)(bp))
#define PUT_PREV(p, val) (*(unsigned int *)(bp) = (val))
#define GET_SUCC(bp) (*(unsigned int *)(bp+1))
#define PUT_SUCC(p, val) (*(unsigned int *)(bp+1) = (val))
/* Dereference void pointer*/
#define PTR_VAL(bp) (*(int*)bp)

/* Macros for mm_check */
#define GET_FLAG(bp) (*(unsigned int *)(bp + 2))
#define PUT_FLAG(bp, val) (*(unsigned int *)(bp + 2) = (val))

static void *find_fit(size_t asize);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);

void *heap_listp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  /* Create the initial empty heap */
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    return -1;

  PUT(heap_listp, 0);                            /* Alignment padding */
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* predeccessor pointer */
  //PUT(heap_listp + (4 * WSIZE), 0); /* successor pointer */
  //PUT(heap_listp + (5 * WSIZE), PACK(0, 1));     /* Epilogue header */
  heap_listp += (2 * WSIZE); // point to first block after prologue

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    return -1;
  return 0;
}

static void *extend_heap(size_t words)
{
  size_t size;
  void *bp;
  /* Allocate an even number of words to maintain alignment */
  // size = ALIGN(words);
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // round up to nearest 2 words
  if ((long)(bp = mem_sbrk(size)) == -1)
  {
    return NULL;
  }
  // receive memory after header o epilogue block -> header of new free block

  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
  PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue block header */
                                        //  PUT(FTRP(PREV_BLKP(bp)), PACK(0, 1));

  /* Coalesce if the previous block was free */
  return coalesce(bp); // return block pointer to merged blocks
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  // mm_check();
  size_t asize;
  size_t extendsize;
  void *bp;

  if (size == 0)
    return NULL;

  if (size <= DSIZE)
  {
    asize = 2 * DSIZE;
  }
  else
  {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }

  /* Search the free list for a fit */
  if ((bp = find_fit(asize)) != NULL)
  {
    place(bp, asize);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    return NULL; // no more memory
  place(bp, asize);
  return bp;
}

static void *coalesce(void *bp)
{

  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc)
  { /* Case 1 */
    // put at the begin of the free list
    return bp;
  }

  else if (prev_alloc && !next_alloc)
  { /* Case 2 */
    //merge with existing 
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  else if (!prev_alloc && next_alloc)
  { /* Case 3 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  else
  { /* Case 4 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 if ptr is NULL, the call is equivalent to mm_malloc(size);
if size is equal to zero, the call is equivalent to mm_free(ptr);
if ptr is not NULL, it must have been returned by an earlier call to mm_malloc or mm_realloc. 
The call to mm_realloc changes the size of the memory block pointed to by ptr (the old block) 
to size bytes and returns the address of the new block. 
Notice that the address of the new block might be the same as the old block, 
or it might be different, depending on your implementation, the amount of internal fragmentation in the old block, 
and the size of the realloc request.
The contents of the new block are the same as those of the old ptr block, 
up to the minimum of the old and new sizes. Everything else is uninitialized. 
For example, if the old block is 8 bytes and the new block is 12 bytes, 
then the first 8 bytes of the new block are identical to the first 8 bytes of the old block and the last 4 bytes are uninitialized. 
Similarly, if the old block is 8 bytes and the new block is 4 bytes, then the contents of the new block are identical to the first 4 bytes of the old block.
*/
void *mm_realloc(void *ptr, size_t size)
{
  if (ptr == NULL){
    return mm_malloc(size);
  }
  if (size == 0){
    mm_free(ptr);
    return NULL;
  }

  //ptr is not null
  size_t current_size = GET_SIZE(HDRP(ptr));
  size_t sizeBis = MAX(ALIGN(size) + DSIZE,MINBLOCKSIZE);
  void *bp;

  if (sizeBis == current_size){
    return ptr;
  }
  
  bp = mm_malloc(sizeBis);
  memcpy(bp,ptr,sizeBis);
  mm_free(ptr);
  return bp;

}

/*Your solution should place the requested block at the beginning of the free block,
splitting only if the size of the remainder would equal or exceed the minimum
block size.*/

static void place(void *bp, size_t asize)
{
  size_t size = GET_SIZE(HDRP(bp)); // size of free block
  int previous = GET_PREV(bp);
  int successor = GET_SUCC(bp);

  if ((size - asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size-asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size-asize, 0));
    if (successor != 0){
      PUT_PREV(successor, bp);
    }
    PUT_SUCC(previous, bp);
    PUT_PREV(bp,previous);
    PUT_SUCC(bp,successor);
  }
  else { // no split
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
    if (successor != 0){
      PUT_PREV(successor, previous);
    }
    PUT_SUCC(previous,successor);

  }
}


/* Implement a find_fit function for the simple allocator described in Section 9.9.12. */
static void *find_fit(size_t asize)
{

  void *bp;

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    {
      return bp;
    }
  }
  return NULL;

}

void mm_check(void)
{

  
}
