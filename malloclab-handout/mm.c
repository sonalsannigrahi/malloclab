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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"
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

#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//* Basic macros: */
#define WSIZE      sizeof(void *) /* Word and header/footer size (bytes) */
#define DSIZE      (2 * WSIZE)    /* Doubleword size (bytes) */
#define CHUNKSIZE  (1 << 12)      /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 2* DSIZE
/*Max value of 2 values*/
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)  (GET(p) & 0x1)


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)  ((void *)(bp) - WSIZE)
#define FTRP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)


//Additional Macros
#define NEXT_BLK(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLK(bp)  ((void *)(bp) - GET_SIZE((void *)(bp) - DSIZE))

#define GET_NEXT_PTR(bp)  (*(char **)(bp + WSIZE))
#define GET_PREV_PTR(bp)  (*(char **)(bp))

#define SET_NEXT_PTR(bp, qp) (GET_NEXT_PTR(bp) = qp)
#define SET_PREV_PTR(bp, qp) (GET_PREV_PTR(bp) = qp)

void *heap_listp = 0; 

/* Function Declarations */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void insert_node(void *bp); 
static void delete_node(void *bp); 

int mm_init(void) {
  
  /* Create the initial empty heap. */
  if ((heap_listp = mem_sbrk(8*WSIZE)) == NULL) 
    return -1;

  PUT(heap_listp, 0);                            /* Alignment padding */
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
  heap_listp += 2*WSIZE; 

  /* Extend the empty heap with a free block of minimum possible block size */
  if (extend_heap(4) == NULL){ 
    return -1;
  }
  return 0;
}



static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  //Since minimum block size given to us is 4 words (ie 16 bytes)
  if (size < MINBLOCKSIZE){
    size = MINBLOCKSIZE;
  }
  /* call for more memory space */
  if ((int)(bp = mem_sbrk(size)) == -1){ 
    return NULL;
  }
  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* free block header */
  PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
  PUT(HDRP(NEXT_BLK(bp)), PACK(0, 1)); /* new epilogue header */
  /* coalesce bp with next and previous blocks */
  return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size) 
{
  size_t asize;      /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  void *bp;

  /* Ignore spurious requests. */
  if (size == 0)
    return (NULL);

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE)
    asize = 2 * DSIZE;
  else
    asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

  /* Search the free list for a fit. */
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return (bp);
  }

  /* No fit found.  Get more memory and place the block. */
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)  
    return (NULL);
  place(bp, asize);
  return (bp);
} 


static void *coalesce(void *bp){

  size_t next_alloc = GET_ALLOC(  HDRP(NEXT_BLK(bp))  );
  size_t prev_alloc = GET_ALLOC(  FTRP(PREV_BLK(bp))  ) || PREV_BLK(bp) == bp;
  size_t size = GET_SIZE(HDRP(bp));
 
  if (prev_alloc && !next_alloc) 
    { /*Case 2 */
    //merge with existing                 
    size += GET_SIZE( HDRP(NEXT_BLK(bp))  );
    delete_node(NEXT_BLK(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  else if (!prev_alloc && next_alloc) 
    { /*Case 3 */              
    size += GET_SIZE( HDRP(PREV_BLK(bp))  );
    bp = PREV_BLK(bp);
    delete_node(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }

  else if (!prev_alloc && !next_alloc) 
    { /*Case 4 */
    size += GET_SIZE( HDRP(PREV_BLK(bp))  ) + GET_SIZE( HDRP(NEXT_BLK(bp))  );
    delete_node(PREV_BLK(bp));
    delete_node(NEXT_BLK(bp));
    bp = PREV_BLK(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }
  insert_node(bp);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp){
  size_t size;
  if (bp == NULL)
    return;
  size = GET_SIZE(HDRP(bp));
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

/* Implement a find_fit function for the simple allocator described in Section 9.9.12. */

static void *find_fit(size_t asize){
  void *bp;
  static int malloc_size = 0;
  static int counter = 0;
  if( malloc_size == (int)asize){
      if(counter>30){  
        int sizeBis = MAX(asize, MINBLOCKSIZE);
        bp = extend_heap(sizeBis/4);
        return bp;
      }
      else
        counter++;
  }
  else
    counter = 0;
  for (bp = heap_listp; GET_ALLOC(HDRP(bp)) == 0; bp = GET_NEXT_PTR(bp) ){
    if (asize <= (size_t)GET_SIZE(HDRP(bp)) ) {
      malloc_size = asize;
      return bp;
    }
  }
  return NULL;
}

/*Your solution should place the requested block at the beginning of the free block,
splitting only if the size of the remainder would equal or exceed the minimum
block size.*/

static void place(void *bp, size_t asize){
  size_t freeSize = GET_SIZE(HDRP(bp));

  if ((freeSize - asize) >= MINBLOCKSIZE) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    delete_node(bp);
    bp = NEXT_BLK(bp);
    PUT(HDRP(bp), PACK(freeSize-asize, 0));
    PUT(FTRP(bp), PACK(freeSize-asize, 0));
    coalesce(bp);
  }
  else {
    PUT(HDRP(bp), PACK(freeSize, 1));
    PUT(FTRP(bp), PACK(freeSize, 1));
    delete_node(bp);
  }
}

static void insert_node(void *bp){
  SET_NEXT_PTR(bp, heap_listp); 
  SET_PREV_PTR(heap_listp, bp); 
  SET_PREV_PTR(bp, NULL); 
  heap_listp = bp; 
}

static void delete_node(void *bp){
  if (GET_PREV_PTR(bp))
    SET_NEXT_PTR(GET_PREV_PTR(bp), GET_NEXT_PTR(bp));
  else
    heap_listp = GET_NEXT_PTR(bp);
  SET_PREV_PTR(GET_NEXT_PTR(bp), GET_PREV_PTR(bp));
}
