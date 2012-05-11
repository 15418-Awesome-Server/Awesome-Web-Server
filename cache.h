/* Modified for use in the 15-418 project by Woody Thompson and Eric Summers */


/* cache.h - A header file for cache.c
 *
 * Authors:
 *   Woody Thompson - wrthomps
 *   Brandon Lee    - bfl
 * 
 * The cache is implemented as a doubly-linked list with a head and tail
 * pointer. The list maintains an implicit order based on time since each
 * object was accessed, by always adding a new object to the front, and moving
 * any accessed object to the front. Eviction then means removing the last
 * object in the list. This means that both cache addition and cache eviction
 * can be done in constant time. One concern with this implementation is that
 * the cache must be locked for writing even on access, to move the accessed
 * object to the front.
 *
 * Testing has shown that this implementation provides better performance than
 * using a timestamp field in the cacheobj struct. We expect that this is due
 * to the 8 threads per process limit imposed on us on the shark machines, so
 * there is never a significant reading/writing queue. With a higher thread
 * limit, or perhaps with no thread limit at all, this implementation will
 * likely experience performance loss as that queue grows.
 *
 * One thing to note is that on the CMU CS website, one request string sent
 * by Mozilla Firefox to the site includes the time of access. Since this
 * implementation relies on using request strings as keys, a portion of the
 * site is therefore impossible to reliably cache using this method.
 */

#include "csapp.h"

#define MAX_CACHE_SIZE  5000000
#define MAX_OBJECT_SIZE 1000000
#define MIN_OBJECT_SIZE 0

/* 
 * Each cache object is stored as a key-value pair. The key is the original
 * request line sent to the proxy, minus any request headers, with version
 * HTTP/1.0. The value is the returned object. Since the object is of any
 * arbitrary type or structure dependent on the request and server, it is 
 * stored as a generic pointer with its size, so that the object is the size 
 * bytes starting at address obj.
 */
typedef struct cacheobj {
  char *req;
  void *obj;
  struct cacheobj *next;
  struct cacheobj *prev;
  unsigned int size;
} cacheobj;

cacheobj *head;              /* Points to the head of the linked list */ 
cacheobj *tail;              /* Points to the tail of the linked list */
unsigned int cache_size; /* Holds the current cache size */

/* Initializes the cache. Returns 0 on success, -1 on error */
int init_cache();

/* Caches the object pointed to by obj. Returns 0 on success, -1 on error */
int cache_object(void *obj, int size, char *req);

/* Adds the specified object to the front of the list and increases the cache
 * size appropriately */
void add_to_front(cacheobj *input);

/* Evicts the specified cache object struct from the cache. Only called
 * internally. Returns a pointer to the item evicted on success, returns 
 * NULL otherwise */
cacheobj *evict_object(cacheobj *victim);

/* Returns a pointer to the object in the cache if it exists, NULL otherwise.
 * Also marks the new access time of the object in the cache by moving it to
 * the front. */
cacheobj *in_cache(char *req);

/* Prints out the keys in the cache starting from the front of the list,
 * for debugging purposes */
void print_cache();
