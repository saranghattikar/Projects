#include "threads/thread.h"
#include "threads/malloc.h"

#include "devices/timer.h"

#include "filesys/filesys.h"
#include "filesys/cache.h"

/* List for caches in file system */
struct list caches_in_file_system;

/* File System Cache Size */
uint32_t size_of_cache_in_file_system;

/* Lock for cache in file system */
struct lock lock_cache_in_file_system;

void cache_init (void)
{
  /* Initialize the size for the cache */
  size_of_cache_in_file_system = 0;

  /*Initialize the cache lock */
  lock_init (&lock_cache_in_file_system);

  /*Initialize the cache list */
  list_init (&caches_in_file_system);

  thread_create ("callback_to_write", 0, callback_to_write, NULL);
}

struct cache_element_entry* update_fs_cache (block_sector_t sector,
bool dirty)
{
  struct cache_element_entry *cache;
  struct list_elem *list_elemnt;

  bool is_break = false;

  /* Loop until the cache element is found */
  while (!is_break)
  {
    /* Iterate over the list of caches in the file system */
    for (list_elemnt = list_begin (&caches_in_file_system);
        list_elemnt != list_end (&caches_in_file_system); list_elemnt =
            list_next (list_elemnt))
    {
      cache = list_entry(list_elemnt, struct cache_element_entry,
          cache_list_elem);
      if (cache->counter > 0)
        continue;

      /* Set the accessed bit of cache in case if it is accessed */
      if (cache->is_cache_accessed)
        cache->is_cache_accessed = false;
      else
      {
        /* Write to the block if cache dirty bit set */
        if (cache->is_cache_dirty)
          block_write (fs_device, cache->cache_block_sector,
              &cache->cache_block);
        is_break = true;
        break;
      }
    }
  }
  // update the cache and return it
  cache->cache_block_sector = sector;

  cache->counter = cache->counter + 1;

  //Read from the block
  block_read (fs_device, cache->cache_block_sector, &cache->cache_block);

  //update the cache bits

  cache->is_cache_accessed = true;
  cache->is_cache_dirty = dirty;

  return cache;
}

struct cache_element_entry* remove_cache (block_sector_t sector,
bool dirty)
{
  struct cache_element_entry *cache;

  /* Check if the size of cache has reached maximum or not */
  if (size_of_cache_in_file_system < 64)
  {
    /* Allocate memory for the cache element */
    cache = malloc (sizeof(struct cache_element_entry));

    /* Update the size */
    size_of_cache_in_file_system = size_of_cache_in_file_system + 1;

    if (!cache)
      return NULL;

    cache->counter = 0;

    /* Add the cache element to the the list */

    list_push_back (&caches_in_file_system, &cache->cache_list_elem);

    // update the cache and return it
    cache->cache_block_sector = sector;

    cache->counter = cache->counter + 1;

    //Read from the block
    block_read (fs_device, cache->cache_block_sector, &cache->cache_block);

    //update the cache bits

    cache->is_cache_accessed = true;
    cache->is_cache_dirty = dirty;

    return cache;

  } else
  {
    /* If cache in file system full then update the filesysytem */
    return update_fs_cache (sector, dirty);
  }

}

struct cache_element_entry* get_cache (block_sector_t block,
bool not_dirty)
{
  lock_acquire (&lock_cache_in_file_system);
  struct list_elem *list_element;
  struct cache_element_entry *cache;

  /* Iterate over the list of elements in the cache */
  for (list_element = list_begin (&caches_in_file_system);
      list_element != list_end (&caches_in_file_system); list_element =
          list_next (list_element))
  {
    cache = list_entry(list_element, struct cache_element_entry,
        cache_list_elem);

    /* Update the cache if the element is found */
    if (cache->cache_block_sector == block)
    {
      /* Update the cache */
      cache->is_cache_dirty |= !not_dirty;

      cache->is_cache_accessed = true;

      cache->counter = cache->counter + 1;

      //Release the cache lock and return the cache
      lock_release (&lock_cache_in_file_system);
      return cache;
    }
  }

  /* If cache not found evict it */
  cache = remove_cache (block, !not_dirty);

  /* Throw a Panic memory error if cache not found */
  if (!cache)
    PANIC("Memory Error");

  //Release the lock
  lock_release (&lock_cache_in_file_system);
  return cache;
}

void callback_to_write (void *aux UNUSED)
{
  while (true)
  {
    /* Set the timer */
    timer_sleep (5 * TIMER_FREQ);
    lock_acquire (&lock_cache_in_file_system);
    struct list_elem *list_element;
    struct cache_element_entry *cache;

    /* Iterate over the list of caches */

    for (list_element = list_begin (&caches_in_file_system);
        list_element != list_end (&caches_in_file_system); list_element =
            list_next (list_element))
    {
      cache = list_entry(list_element, struct cache_element_entry,
          cache_list_elem);

      /* Update the cache bit and write block in case of true dirty bit */
      if (cache->is_cache_dirty)
      {
        block_write (fs_device, cache->cache_block_sector, &cache->cache_block);
        cache->is_cache_dirty = false;
      }
    }
    lock_release (&lock_cache_in_file_system);
  }
}

/**
 * Flush cache to disk
 */
void flush_cache (void)
{
  lock_acquire (&lock_cache_in_file_system);
  struct list_elem *list_element;
  struct cache_element_entry *cache;
  struct list_elem *present_elem = list_begin (&caches_in_file_system);

  /* Iterate over the list of caches */

  while (present_elem != list_end (&caches_in_file_system))
  {
    cache = list_entry(present_elem, struct cache_element_entry,
        cache_list_elem);

    /* Update the cache bit and write block in case of true dirty bit */

    if (cache->is_cache_dirty)
    {
      block_write (fs_device, cache->cache_block_sector, &cache->cache_block);
      cache->is_cache_dirty = false;
    }
    list_element = list_next (present_elem);

    /* Remove the cache from the list and free it after writing to disk */
    list_remove (&cache->cache_list_elem);

    /* Free the cache */
    free (cache);
    present_elem = list_element;
  }
  lock_release (&lock_cache_in_file_system);
}

