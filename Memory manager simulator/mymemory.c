#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "memory.h"

/***************************************/


//GLOBALS

node_t * freehead = NULL;

pthread_mutex_t lock;

//MACROS

#define BLOCK_SIZE 16

#define ALIGN8(x) ( (~7)&((x)+7) )


/***************************************/


/* mymalloc_init: initialize any data structures that your malloc needs in
 *                 order to keep track of allocated and free blocks of 
 *                 memory.  Get an initial chunk of memory for the heap from
 *                 the OS using sbrk() and mark it as free so that it can  be 
 *                 used in future calls to mymalloc()
 */

int mymalloc_init() {
  
  
 /* initalizes the global locks that the threads will use when they are 
  * mallocing, freeing or coalescing the global linked free list 
  */
  pthread_mutex_init(&lock, NULL); // initalizes the lock
  void * END_ADDRESS; // for error checking

  
  //initailizes our "heap" by one page (4096 bytes)
  freehead = sbrk(0);
  END_ADDRESS = sbrk(4096);   
  
  freehead->size = 4096 - BLOCK_SIZE;
  freehead->next = NULL;
  freehead->free = 0; 
	
  if ( END_ADDRESS == (void *) -1) {
    return 1; // non-zero return value indicates an error
  
  }

  return 0;

}


/*  mymalloc: Takes an unsigned int size, then calls malloc_lock which will
 *            allocate memory in our "heap" and returns back a pointer to that
 *            space for the caller  
 */

void * mymalloc(unsigned int size) {
  
  
  void * return_ptr;
     
  pthread_mutex_lock(&lock); //only one thread is allowed to malloc, since it changes our global list   
     
  return_ptr = malloc_lock(size); 
    
  pthread_mutex_unlock(&lock);
     
  return return_ptr;

}


/*  malloc_lock: helper function for mymalloc, takes an unsigned int and allocates a total of the size
 *               of the call plus the HEADER_SIZE. The function first searches for a large enough free 
 *               block by first fit from our free list. It will write the header information in the header
 *               space then return the free space just after the header back to the caller. The free block 
 *               that was written on will be split, and the resulting new free block will be appended back 
 *               to the front of the list 
 */

void * malloc_lock(unsigned int size){

  void * ret_ptr = NULL;
  node_t * prevPtr = NULL;
  node_t * currPtr;
  currPtr = freehead;

  /* For 1st node, indicate there is no previous. 
   * Visit each node, maintaining a pointer to
   * the previous node we just visited.
   */

  while (currPtr != NULL) {
    
    if ((currPtr->size >= ( ALIGN8(size) + BLOCK_SIZE )) && currPtr->free == 0) {
	
      if (prevPtr == NULL) { // This means that the free block we found is the head of the list 
	  
	node_t * newPtr;
	
	newPtr = (node_t *)((char *)currPtr + ALIGN8(size) + BLOCK_SIZE); // The free block is split into 2 pieces.
	                                                                  // The start of the new block is pointed to 
	                                                                  // by newPtr (Found using pointer arithmetic)
	
	newPtr->size = currPtr->size - ALIGN8(size) - BLOCK_SIZE; // write the necessary info for the new blocks
	newPtr->free = 0; // 0 means the block is "free"
	
	newPtr->next = freehead->next; // insert new free block to beginning of list	
	freehead = newPtr;

	currPtr->size = size;
	currPtr->free = 1; // 1 means not "free" 
	ret_ptr = (void *)((char *)currPtr + BLOCK_SIZE);
	  
	return ret_ptr;	
      }
	
      else { // (prevPtr != NULL) //means that the free block we found suitable is in the middle or end of the list
	  
	node_t * newPtr;
	
	newPtr = (node_t *)((char *)currPtr + ALIGN8(size) + BLOCK_SIZE);
	newPtr->size = currPtr->size - ALIGN8(size) - BLOCK_SIZE;
	newPtr->free = 0;
  
	newPtr->next = freehead;
	freehead = newPtr;
	prevPtr->next = currPtr->next;
	
	currPtr->size = size;
	currPtr->free = 1;
	ret_ptr = (void *)((char *)currPtr + BLOCK_SIZE);
	  
	return ret_ptr;	
	
	}
	
    }
    
    prevPtr = currPtr;
    currPtr = currPtr->next;
    
  }
  
  if (increase_heap() == -1) {    //the code will reach here if there is not enough usuable heap space
                                  // this increases the size of our "usuable" heap by one page (4096 bytes)  
	  return NULL;
	  
  }
	
  ret_ptr = malloc_lock(size); //calls malloc_lock again, since we know that it can only reach here
			     // if the heap has increased. We call malloc again with an increased heap 
  
  return ret_ptr;	
  
}   
      
     
/*  increase_heap: This function increases the heapspace by a new page (4096 bytes) and adds it as a free block
 *                 into our list. It uses 16 of said bytes to create a header for the newly made free block.
 *                 The function then calls coalesce(), which will merge any blocks adjacent in memory with the
 *                 free block into one bigger block
 */

int increase_heap() {
	
  void * END_ADDRESS;
  
  node_t * newPtr;
  
  newPtr = sbrk(0);
	
  END_ADDRESS = sbrk(4096);	
  if ( END_ADDRESS == (void *) -1) { // error checking
    
    return -1;
  
  }
	
  newPtr->size = 4096 - BLOCK_SIZE;

  coalesce(newPtr, 1); //calls coalesce to merge with adjacent free blocks

  return 0;
  
}


/*  Coalesce: Coalesce uses helper functions to find the left adjacent and right adjacent of a 
 *            free block in memory. The blocks are then merged together and added to our global 
 *            free list. We use the lowest numbered header address as the new head of our merged block;
 *            the left adjacent will always have the smallest, followed by the newly current freed block
 *            then the right adjacent. IN HEADER ADDRESSES: (left < current < right)
 */

int coalesce(node_t * current, int isheap){


  node_t * leftAdj = NULL;
  node_t * rightAdj = NULL;
  
  leftAdj = find_leftAdj(current); //finds the free left adjacent, returns the null if the block is not free
  
  if (isheap != 1) { // a newly created page wont have a right adjacent in memory, thus to save time dont try to find
		    // it
		    
    rightAdj = find_rightAdj(current); // finds the free right adjacent, returns the null if the block is not free
    
  }

  if (leftAdj && rightAdj) {  // if there are both left and right adjacent free blocks in memory
                              // merge all 3 blocks togther, and take the left Adjacent's blocks place in
                              // the free list. Remove the right adjacent freeblock from the freelist.
    if (rightAdj == freehead) {
     
      freehead = freehead->next; // remove the right adjacent block from the freelist
    }
    
    else {
      
      node_t * changeNextPtr;
      
      changeNextPtr = find_prevAdj(rightAdj); // calls prevAdj which is used to properly re-link the 
                                              // global linked free-list
      changeNextPtr->next = rightAdj->next;
    }
    
    leftAdj->size = ALIGN8(leftAdj->size) + ALIGN8(current->size) + ALIGN8(rightAdj->size) + (2*BLOCK_SIZE);
    // update the new size for the left Adjacent block after it has merged together with both blocks
    return 0;
  }
  
  else if (leftAdj) {  // if there are only the left adjacent free blocks in memory
                       // merge both blocks togther, and take the left Adjacent's blocks place in
                       // the free list. 
  
    leftAdj->size = ALIGN8(leftAdj->size) + ALIGN8(current->size) + BLOCK_SIZE;
    return 0;
  }
  
  else if (rightAdj) { // if there are only the right adjacent free blocks in memory
                       // merge both blocks togther. We will take the newly free block,
                       // update its size after the merge with it's right adjacent and 
                       // add it into our freelist
    
    if (rightAdj == freehead) {
      
      freehead = current;
      current->next = rightAdj->next;
    }
    
    else {
      
      node_t * changeNextPtr;
      
      changeNextPtr = find_prevAdj(rightAdj);
      changeNextPtr->next = current;
      current->next = rightAdj->next;
    }
    
    current->size = ALIGN8(current->size) + ALIGN8(rightAdj->size) + BLOCK_SIZE;
    return 0;
  }
  
  else { // no adjacent blocks, just add the newly freed block to the front of the list.
    
      current->next = freehead;
      freehead = current;
  
      return 0;
  }
  
}


/*  find_leftAdj HELPER: helper function for coalesce(). Finds the left adjacent of the given
 *                       block. Returns a pointer to the left adjacent block
 */

node_t * find_leftAdj (node_t * currPtr) {
  
  node_t * markPtr;
  node_t * ret_Ptr = NULL;
  
  markPtr = freehead;
  
  while (markPtr != NULL){
    
    if ((markPtr->free == 0) && (currPtr == (node_t *)((char *)markPtr + BLOCK_SIZE + ALIGN8(markPtr->size))) ) {
      ret_Ptr = markPtr;  
      
      return ret_Ptr;
    }
    
  markPtr = markPtr->next;  
  }
  
  return ret_Ptr;
}


/*  find_rightAdj HELPER: helper function for coalesce(). Finds the right adjacent of the given
 *                       block. Returns a pointer to the right adjacent block
 */

node_t * find_rightAdj (node_t * currPtr) {

  node_t * nextPtr;
  
  nextPtr = ((node_t *)((char *)currPtr + BLOCK_SIZE + ALIGN8(currPtr->size)));
  
  if (nextPtr->free == 0) {
   
    return nextPtr;      
  }
  
  return NULL;  
}


/*  find_prevAdj HELPER: helper function for coalesce(). Finds the previous block of a 
 *                       block in the freelist.
 */

node_t * find_prevAdj (node_t * currPtr) {
  
  node_t * markPtr;
  
  markPtr = freehead;
  
  while (markPtr != NULL){
  
    if (markPtr->next == currPtr) {
      
      return markPtr;
      
    }
  
  markPtr = markPtr->next;  
  }
  
  return NULL;
}
  

/* myfree: calls free_lock to help unallocate memory 
 *         Only one thread can call free_lock at one time, since freeing memory will
 *         change the global linked free-list
 */

unsigned int myfree(void *ptr) {
	
  unsigned int num;
  
  pthread_mutex_lock(&lock);
	
  num = free_lock(ptr);

  pthread_mutex_unlock(&lock);
	  
  return num;
}


/* free_lock: unallocates memory that has been allocated with mymalloc.
 *            gives it a void pointer to the first byte of a block of memory allocated by 
 *            mymalloc.
 *            returns 0 if the memory was successfully freed and 1 otherwise.
 */

unsigned int free_lock(void *ptr){
  
  node_t * freePtr;

  freePtr = (node_t *)((char *)ptr - BLOCK_SIZE);

  if (freePtr->free == 1) {
	  
    freePtr->free = 0; // changes this block to free
    coalesce(freePtr, 0); // this new free block could be merged with other free blocks, call coalesce to check
    return 0;
  }

  return 1;
}

