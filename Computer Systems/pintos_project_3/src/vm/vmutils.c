/*
 * vmutils.c
 * Created on: Mar 27, 2015
 * Author: Shruthi, Pramod
 */
#include <stdio.h>
#include <stddef.h>
#include "vm/page.h"
#include "vm/vmutils.h"
#include "vm/frame.h"

/**
 * Evict physical frame if non of frames are free
 * Uses basic Clock Eviction Algorithm
 */
struct supplementary_frame_in_memory *evict_frame (
    struct supplementary_page_table_entry *page,
    size_t present_frames,
    struct supplementary_frame_in_memory *global_frame,
    size_t frame_count)
{
  int i = 0;
  int counter = frame_count * 2;
  while (i < counter)
  {
    i++;

    struct supplementary_frame_in_memory *local_frame =
        &global_frame [present_frames];
    if (frame_count <= ++present_frames)
      present_frames = 0;

    if (!lock_try_acquire (&local_frame->this_lock))
      continue;

    if (local_frame->process_pg == NULL)
    {
      local_frame->process_pg = page;
      return local_frame;
    }

    if (is_page_accessed (local_frame->process_pg))
    {
      lock_release (&local_frame->this_lock);
      continue;
    }

    if (!evict_page (local_frame->process_pg))
    {
      lock_release (&local_frame->this_lock);
      return NULL;
    }

    local_frame->process_pg = page;
    return local_frame;
  }
  return NULL;
}

/**
 * Iterate over the global frames array to find if any free frame is available
 * If available - allocate this frame to given virtual page
 */
struct supplementary_frame_in_memory *check_for_free_frame (
    struct supplementary_page_table_entry *page,
    struct supplementary_frame_in_memory *global_frame,
    size_t frame_count)
{
  int i = 0;
  while (i < frame_count)
  {
    i++;
    struct supplementary_frame_in_memory *local_frame = &global_frame [i];
    // if we can't get the lock over the frame meaning it is getting used currently,
    // so leave this frame - its not available
    if (!lock_try_acquire (&local_frame->this_lock))
    {
      continue;
    }
    // if frame is not associated with any page
    if (local_frame->process_pg == NULL)
    {
      local_frame->process_pg = page;
      return local_frame;
    }
    lock_release (&local_frame->this_lock);
  }
  return NULL;
}

/* COMPARATOR function for hashmap data-structure, compares the hash values
 * function name - hash_values_comparator
 * Compares the virtual addresses of given two suppplimentary_page entries
 */
bool hash_values_comparator (const struct hash_elem *elem1,
    const struct hash_elem *elem2, void *aux UNUSED)
{
  struct supplementary_page_table_entry *p1
        = hash_entry(elem1, struct supplementary_page_table_entry, elem);
  struct supplementary_page_table_entry *p2
        = hash_entry(elem2, struct supplementary_page_table_entry, elem);

  return ( p1->vaddr < p2->vaddr );
}

/**
 * Get the Hash value for suppplimentary_page given by has_elem
 */
unsigned get_hash_value_for_page (const struct hash_elem *h_elem,
    void *aux UNUSED)
{
  struct supplementary_page_table_entry *page
        = hash_entry(h_elem, struct supplementary_page_table_entry, elem);
  return hash_bytes (&page->vaddr, sizeof ( page->vaddr ));
}
