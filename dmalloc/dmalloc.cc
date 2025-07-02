#include <sys/wait.h>
#define DMALLOC_DISABLE 1
#include "dmalloc.hh"
#include <cassert>
#include <cstring>

struct memory_tracker tracker;

/**
 * dmalloc(sz,file,line)
 *      malloc() wrapper. Dynamically allocate the requested amount `sz` of memory and 
 *      return a pointer to it 
 * 
 * @arg size_t sz : the amount of memory requested 
 * @arg const char *file : a string containing the filename from which dmalloc was called 
 * @arg long line : the line number from which dmalloc was called 
 * 
 * @return a pointer to the heap where the memory was reserved
 */
void* dmalloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    ;

    char* allocation;
    size_t total_size = sz + 2; // 2 for magic bytes

    // Check for unsigned addition overflow ( INTEGER overflow )
    if(total_size < sz)
    {
      allocation = NULL;
    }else{
      allocation = (char *)base_malloc(total_size);
    }

    if(!allocation)
    {
      tracker.nfail += 1;
      tracker.fail_size += sz;
      return NULL;
    }
    
    tracker.ntotal += 1;
    tracker.total_size += sz;
    tracker.nactive += 1;
    tracker.active_size += sz;

    allocation[sz] = MAGIC_3; // don't do sz+1 and sz+2 here as indexing starts from 0 to sz-1 ( total sz elements )
    allocation[sz+1] = MAGIC_4;

    uintptr_t heap_min = (uintptr_t)allocation;
    uintptr_t heap_max = (uintptr_t)(allocation) + sz;
    if(!tracker.heap_min || heap_min <= tracker.heap_min)
    {
      tracker.heap_min = heap_min;
    }
    if(!tracker.heap_max || heap_max >= tracker.heap_max)
    {
      tracker.heap_max = heap_max;
    }

    if(tracker.active_allocations_head == NULL)
    {
      // Create a head node
      node_t* head = (node_t*)malloc(sizeof(node_t));
      head->next = NULL;
      head->data = (void *)allocation;
      head->size = sz;
      tracker.active_allocations_head = head;
    }
    else{
      // Add a node at the beginning of the list
      node_t* head = (node_t*)malloc(sizeof(node_t));
      head->data = (void *)allocation;
      head->next = tracker.active_allocations_head;
      head->size = sz;
      tracker.active_allocations_head = head;
    }

    return (void*)allocation;
}

/**
 * dfree(ptr, file, line)
 *      free() wrapper. Release the block of heap memory pointed to by `ptr`. This should 
 *      be a pointer that was previously allocated on the heap. If `ptr` is a nullptr do nothing. 
 * 
 * @arg void *ptr : a pointer to the heap 
 * @arg const char *file : a string containing the filename from which dfree was called 
 * @arg long line : the line number from which dfree was called 
 */
void dfree(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    if(!ptr)
      return;

    bool allocated = false;
    bool in_heap_range = false;
    // Check if re-allocated on heap, the same memory
    node_t* allocated_list = tracker.active_allocations_head;
    node_t* allocated_head = allocated_list;
    node_t* prev_allocated_head = NULL;
    while(allocated_head != NULL)
    {
      if(allocated_head->data == ptr)
      {
        allocated = true;
        break;
      }
      if(uintptr_t(ptr) <= uintptr_t(allocated_head->data) + allocated_head->size && uintptr_t(ptr) >= uintptr_t(allocated_head->data))
        in_heap_range = true;

      prev_allocated_head = allocated_head;
      allocated_head = allocated_head->next;
    }


    // Check if already freed
    node_t* freed_list = tracker.freed_allocations_head;
    node_t* freed_head = freed_list;
    while(freed_head != NULL)
    {
      if(freed_head->data == ptr && !allocated)
      {
        // Already freed
        fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, double free\n", file, line, ptr);
        return;
      }
      freed_head = freed_head->next;
    }

    // Check if not allocated (base pointer) but exist in heap ( allocated range )
    if(!allocated && in_heap_range)
    {
      // valid heap memory but not allocated, exit.
      fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not allocated\n", file, line, ptr);
      return;
    }

    // This pointer is neither base pointer nor inside allocated heap range
    if(!allocated)
    {
      // Not inside allocated memory on heap, exit.
      fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not in heap\n", file, line, ptr);
      return;
    }


    // Remove from allocated list and assign to freed list
    size_t sz = allocated_head->size;
    if(allocated){
      if(allocated_head->data == ptr)
      {
        // Found on allocated list.
        if(prev_allocated_head != NULL)
          // Change prev allocated head, if it's not null
          prev_allocated_head->next = allocated_head->next;
        else 
          // Change the active allocations head
          tracker.active_allocations_head = allocated_head->next;

        // Add to freed list
        if(tracker.freed_allocations_head == NULL)
        {
          // Create a node at the head.
          allocated_head->next = NULL;
          tracker.freed_allocations_head = allocated_head;
        }
        else {
          // Add a node at the head.
          allocated_head->next = tracker.freed_allocations_head;
          tracker.freed_allocations_head = allocated_head;
        }
      }
    }


    // Check for corrupted memory around boundary conditions.
    char* base_ptr = (char *)ptr;
    if((int)base_ptr[sz] != MAGIC_3 || (int)base_ptr[sz+1] != MAGIC_4)
    {
      fprintf(stderr, "MEMORY BUG: %s:%ld: detected wild write during free of pointer %p\n", file, line, ptr);
      abort();
    }

    // Now freeing the pointer.

    tracker.nactive -= 1;
    tracker.active_size -= sz;
    base_free((void *)base_ptr);
}

/**
 * dcalloc(nmemb, sz, file, line)
 *      calloc() wrapper. Dynamically allocate enough memory to store an array of `nmemb` 
 *      number of elements with wach element being `sz` bytes. The memory should be initialized 
 *      to zero  
 * 
 * @arg size_t nmemb : the number of items that space is requested for
 * @arg size_t sz : the size in bytes of the items that space is requested for
 * @arg const char *file : a string containing the filename from which dcalloc was called 
 * @arg long line : the line number from which dcalloc was called 
 * 
 * @return a pointer to the heap where the memory was reserved
 */
void* dcalloc(size_t nmemb, size_t sz, const char* file, long line) {

    // Check for unsigned multiplication overflow. ( Integer overflow )
    if(sz != 0 && nmemb > UINT64_MAX / sz)
    {
      // update the fail count and fail size
      tracker.nfail += 1;
      tracker.fail_size += nmemb * sz;
      return NULL;
    }

    void* ptr = dmalloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}

/**
 * get_statistics(stats)
 *      fill a dmalloc_stats pointer with the current memory statistics  
 * 
 * @arg dmalloc_stats *stats : a pointer to the the dmalloc_stats struct we want to fill
 */
void get_statistics(dmalloc_stats* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(dmalloc_stats));
    // Your code here.

    stats->nactive = tracker.nactive;
    stats->active_size = tracker.active_size;
    stats->ntotal = tracker.ntotal;
    stats->total_size = tracker.total_size;
    stats->nfail = tracker.nfail;
    stats->fail_size = tracker.fail_size;
    stats->heap_min = tracker.heap_min;
    stats->heap_max = tracker.heap_max;
}

/**
 * print_statistics()
 *      print the current memory statistics to stdout       
 */
void print_statistics() {
    dmalloc_stats stats;
    get_statistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

/**  
 * print_leak_report()
 *      Print a report of all currently-active allocated blocks of dynamic
 *      memory.
 */
void print_leak_report() {
    // Your code here.
}
