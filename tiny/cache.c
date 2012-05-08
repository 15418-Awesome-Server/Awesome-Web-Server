/* Modified for use for the 15-418 project by Woody Thompson and Eric Summers */

/* cache.c - A cache implemented as a doubly-linked list ordered by time of
 *           access. Items are added to the front of the list and evicted from
 *           the end of it.
 *
 * Authors:
 *   Woody Thompson - wrthomps
 *   Brandon Lee    - bfl
 */

#include <stdlib.h>
#include "cache.h"
#include "csapp.h"

/* init_cache - Initializes the cache. Sets global variables to their default
 *              states
 *
 *              Returns 0 on success, -1 on error
 */
int init_cache()
{
  head = (cacheobj *)NULL;
  tail = (cacheobj *)NULL;
  cache_size = 0;

  return 0;
}

/* cache_object - Locks the cache and begins writing to it. Adds the object to
 *                the front of the cache.
 *
 *                Returns 0 on success, -1 on error, 1 if nothing was cached
 */
int cache_object(void *obj, int size, char *req)
{
  cacheobj *input;
  cacheobj *victim;

  if (size > MAX_OBJECT_SIZE || size < MIN_OBJECT_SIZE)
    return 1;

  /* Checks to see if items need to be evicted. Items are evicted until there 
   * is enough space for the added cacheobj
   * The size semaphore must be used to lock access to the size variable
   * whenever it is read */
  while((size + cache_size) > MAX_CACHE_SIZE)
  {
    victim = evict_object(tail);

    /* Receiving a null pointer here should be impossible, but in the interest
     * of integrity, it's worth it to check */
    if(victim)
    {
      free(victim->obj);
      free(victim);
    }
  }

  /* Malloc a new cacheobj struct */
  if (!(input = (cacheobj *) malloc(sizeof(cacheobj))))
  {
    printf("Unable to malloc cacheobj \n");
    return -1;
  }

  /* Malloc enough space for the request string */
  if (!(input->req = (char *)malloc(strlen(req)+1)))
  {
    printf("Unable to malloc request string\n");
    return -1;
  }

  /* Malloc to copy the requested object */
  if (!(input->obj = (void *)malloc(size)))
  {
    printf("Unable to malloc object\n");
    return -1;
  }

  /* Construct the values in the new struct */
  memcpy(input->obj, obj, size);
  strcpy(input->req, req);
  input->size = size;
  input->prev = NULL;

  add_to_front(input);

  printf("Cache size: %d\n", cache_size);

  return 0;
}

/* add_to_front - Adds the specified input object to the front of the list */
void add_to_front(cacheobj *input)
{
  input->prev = NULL;

  input->next = head;
  if(head) head->prev = input;
  head = input;
  if(!tail) tail = input;

  cache_size += input->size;
}

/* evict_object - Evicts the specified cache object struct from the cache and
 *                changes the appropiate previous and last pointers
 *
 *                Returns 0 on success, -1 on error 
 */
cacheobj *evict_object(cacheobj *victim)
{
  if(victim == NULL)
    return NULL;

  /* If the evicted object is not the head, makes previous cacheobj point to 
   * victim's next */
  if (victim->prev != NULL)
    victim->prev->next = victim->next;
  
  /* If the evicted object is not the tail, makes next cacheobj point to the
   * victim's previous */
  if (victim->next != NULL)
    victim->next->prev = victim->prev;

  /* Update the head and tail pointers appropriately */
  if(head == victim)
    head = victim->next;
  if(tail == victim)
    tail = victim->prev;

  /* Update the cache size */
  cache_size -= victim->size;

  return victim; 
}

/* in_cache - Looks in the cache to see if the key is inside the cache 
 *
 *            If it is in the cache, then it returns a pointer to the object
 *            itself, otherwise returns NULL
 *
 *            Implements a solution to the first readers-writers problem
 */
cacheobj *in_cache(char *req)
{
  cacheobj *iter, *match;

  /* If there's nothing in the cache, return nothing */
  if (head == NULL)
  {
    return NULL;
  }
 
  match = NULL; 
  iter = (cacheobj *)head;

  /* Iterate through the cache. If the key matches the key inside the cache,
   * saves the pointer */
  while (iter != NULL)
  {
    if(!strcmp(req, iter->req))
    {
      match = iter;
      break;
    }

    iter = iter->next;
  }

  /* If the item was found in the cache, lock the cache, evict it, unlock the
   * cache, and add it back */
  if(match)
  {
    evict_object(match);
    add_to_front(match);
  }

  /* If something is found, the pointer to the cache is returned. Otherwise
   * NULL is returned */
  return match;
}

/* print_cache - Prints out the keys in the cache, starting from the front.
 *               Allows no access to any cache resources while printing.
 *
 *               Used during debugging.
 */
void print_cache()
{
  cacheobj *iter = head;

  while(iter)
  {
    printf("[%s]\n", iter->req);
    iter = iter->next;
  }
}
