/*       DATA STRUCTURES         */

typedef struct ___node_t {
  int size;
  int free; //0 if free, 1 in use
  struct ___node_t * next;
} node_t;

/*     FUNCTION PROTOTYPES       */

int mymalloc_init(void);           // Returns 0 on success and >0 on error.
void *mymalloc(unsigned int size); // Returns NULL on error.
void * malloc_lock(unsigned int size);

unsigned int myfree(void *ptr); 
unsigned int free_lock(void *ptr);

int increase_heap();

int coalesce(node_t * current, int isheap);

node_t * find_leftAdj (node_t * currPtr);
node_t * find_rightAdj (node_t * currPtr);
node_t * find_prevAdj (node_t * currPtr);