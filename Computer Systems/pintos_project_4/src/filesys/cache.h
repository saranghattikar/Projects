#include "threads/synch.h"
#include "devices/block.h"
#include <list.h>

/* Structure for each cache entry element */
struct cache_element_entry
{
  int counter;
  struct list_elem cache_list_elem;
  uint8_t cache_block [BLOCK_SECTOR_SIZE];
  block_sector_t cache_block_sector;bool is_cache_accessed;bool is_cache_dirty;
};

void cache_init (void);
struct cache_element_entry* get_cache (block_sector_t block,
bool not_dirty);
void flush_cache (void);
void callback_to_write (void *aux);

