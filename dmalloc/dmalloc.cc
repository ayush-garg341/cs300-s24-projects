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

    metadata_tracker *metadata;
    size_t total_size = sizeof(metadata_tracker) + sz;

    // Check for unsigned addition overflow ( INTEGER overflow )
    if(total_size < sz)
    {
      metadata = NULL;
    }else{
      metadata = (metadata_tracker *)base_malloc(total_size);
    }

    if(!metadata)
    {
      tracker.nfail += 1;
      tracker.fail_size += sz;
      return NULL;
    }

    metadata->size = sz;
    
    tracker.ntotal += 1;
    tracker.total_size += sz;
    tracker.nactive += 1;
    tracker.active_size += sz;

    uintptr_t heap_min = (uintptr_t) (metadata + 1);
    uintptr_t heap_max = ((uintptr_t) (metadata + 1)) + sz;
    if(!tracker.heap_min || heap_min <= tracker.heap_min)
    {
      tracker.heap_min = heap_min;
    }
    if(!tracker.heap_max || heap_max >= tracker.heap_max)
    {
      tracker.heap_max = heap_max;
    }
    return (void *)(metadata + 1);
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

    metadata_tracker* meta = ((metadata_tracker*)ptr) - 1;
    tracker.nactive -= 1;
    tracker.active_size -= meta->size;
    base_free(meta);
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
