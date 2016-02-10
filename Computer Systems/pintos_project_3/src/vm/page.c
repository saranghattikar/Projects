/*
 *  page.c
 *  Created on: Mar 27, 2015
 *  Author: Shruthi
 *  Modified By ; Prmaod Khare
 */

#include <stdio.h>
#include <string.h>
#include <hash.h>

#include "vm/page.h"
#include "vm/vmutils.h"
#include "vm/frame.h"
#include "vm/swap.h"

#include "userprog/pagedir.h"
#include "userprog/process.h"

#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#include "filesys/file.h"

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

// Maximum size to which stack can grow in bytes => 1MB
#define STACK_SIZE_LIMIT (1024 * 1024)

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/**
 * Check if this page was accessed recently
 * Returns true if the PTE for virtual page VPAGE in PD has been
 * accessed recently, that is, between the time the PTE was
 * installed and the last time it was cleared.  Returns false if
 * PD contains no PTE for VPAGE.
 */
bool is_page_accessed (struct supplementary_page_table_entry *page)
{
  ASSERT(page -> supplementary_frame_in_memory != NULL)
  ASSERT(
      lock_held_by_current_thread (
          &page->supplementary_frame_in_memory->this_lock));
  bool access = pagedir_is_accessed (page->sup_thread->pagedir, page->vaddr);
  if (access)
  {
    pagedir_set_accessed (page->sup_thread->pagedir, page->vaddr, false);
  }
  return access;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/**
 * Loads the frame associated with the given virtual page with actual data either
 * from associated file or swapped disk or just zeroes
 */
bool load_frame_with_data (struct supplementary_page_table_entry *page)
{
  // Allocate given supplementary page table entry -> a new physical memory frame
  page->supplementary_frame_in_memory = allot_frame_for_page_in_memory (page);
  // No frames are avaible - or unable to evict any frame
  if (page->supplementary_frame_in_memory == NULL)
  {
    return false;
  }

  // if this page is associated with any file
  if (page->sup_file != NULL)
  {
    // Read rw_bytes into the physical frame
    off_t read_bytes = file_read_at (
        page->sup_file,
        page->supplementary_frame_in_memory->base_vir_address,
        page->rw_bytes,
        page->file_offset);

    // Copy this bytes into physical memory frame
    memset (page->supplementary_frame_in_memory->base_vir_address + read_bytes,
        0, PGSIZE - read_bytes);

    if (read_bytes != page->rw_bytes)
    {
      printf ("the number of bytes read are (%"PROTd") != number of bytes "
      "requested are (%"PROTd")\n", read_bytes, page->rw_bytes);
    }
  // Else check if physical frame was swapped to disk
  } else if (page->area_swap != (block_sector_t) -1)
  {
    // If yes, swap it back into physical frame memory
    swap_page_in_from_disk_to_frame (page);
  // Else write zeroes to the frame memory
  } else
  {
    memset (page->supplementary_frame_in_memory->base_vir_address, 0, PGSIZE);
  }

  // Return the operation status - i.e. it was successful
  return true;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * Maps the given user virtual address to the hash table's supplementary_page_table_entry.
 * i.e. Allocate the new supplementary_page_table_entry object and map it with
 * rounded down value of given virtual address
 */
struct supplementary_page_table_entry* allocate_supplementary_PTE (
    void * vir_addr,
    bool write_only)
{
  struct thread *current = thread_current ();
  struct supplementary_page_table_entry *page = malloc (sizeof ( *page ));
  if (page == NULL)
  {
    return page;
  }

  /* Initialize supplementary_page_table_entry with default values and given virtual address*/
  page->sup_thread = current;
  page->supplementary_frame_in_memory = NULL;
  page->file_offset = 0;
  page->vaddr = pg_round_down (vir_addr);
  page->is_readonly = !write_only;
  page->swap_write = write_only;
  page->area_swap = (block_sector_t) -1;
  page->rw_bytes = 0;
  page->sup_file = NULL;

  // Insert this supplementary page table entry into supplementary page table
  if (hash_insert (current->supplementary_page_table, &page->elem) != NULL)
  {
    // If failed - release the object and return NULL
    free (page);
    page = NULL;
  }
  return page;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/**
 * Get supplementary_page_table_entry for given virtual address ->
 * First finds it into the supplementary_page_table, if found returns
 * supplementary_page_table_entry object.
 * Else if its a new page for stack auto growth then try to allocate the new page,
 * Else if its not a stack-page just return NULL.
 */
static struct supplementary_page_table_entry *get_page_table_entry (const void *vaddr)
{
  if (vaddr >= PHYS_BASE)
  {
    return NULL;
  }
  struct thread *current = thread_current ();
  struct supplementary_page_table_entry page1;
  struct hash_elem *elem;
  page1.vaddr = (void *) pg_round_down (vaddr);
  elem = hash_find (current->supplementary_page_table, &page1.elem);
  /* If page is present return the hash entry. */
  if (elem != NULL)
  {
    return hash_entry(elem, struct supplementary_page_table_entry, elem);
  } else
  {
    // Check the virtual address of stack page within 1MB - maximum stack limit
    if ( ( vaddr >= ( PHYS_BASE - STACK_SIZE_LIMIT ) )
        // also check if the current page has less than 32 bytes of space remaining
        && ( vaddr >= ( current->stack_ptr - 32 ) ))
    {
      // if yes allocate a new page for stack
      return allocate_supplementary_PTE ((void *) vaddr, true);
    }
  }
  return NULL;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/**
 * Check if for the given virtual address, there is a supplementary_page_table_entry
 * exists in the page table
 */
bool check_faulted_page_for_virtual_address (void *fault_addr)
{
  // Get current thread structure reference
  struct thread *current = thread_current ();
  struct supplementary_page_table_entry *page;
  bool success;

  // If there is no page table for current thread then something is wrong - return false
  if (current->supplementary_page_table == NULL)
  {
    return false;
  }

  // Get the supplementary_page_table_entry from the page table
  page = get_page_table_entry (fault_addr);
  // If there is no virtual page entry then return false
  if (page == NULL)
  {
    return false;
  }

  // Get the lock of this page's frame
  lock_page_frame (page);
  // If there is no frame associated with this page then allocate new frame
  if (page->supplementary_frame_in_memory == NULL)
  {
    // If loading new frame with actual data fails, return false
    if (!load_frame_with_data (page))
    {
      return false;
    }
  }

  ASSERT(
      lock_held_by_current_thread (
          &page->supplementary_frame_in_memory->this_lock));

  // Set this page into pagedir
  success = pagedir_set_page (current->pagedir, page->vaddr,
      page->supplementary_frame_in_memory->base_vir_address,
      !page->is_readonly);

  lock_release (&page->supplementary_frame_in_memory->this_lock);
  return success;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/**
 * Get the supplementary_page_table_entry for given virtual address if any and
 */
bool lock_and_write_page (const void *addr, bool write)
{
  struct supplementary_page_table_entry *page = get_page_table_entry (addr);
  if (page == NULL || ( write && page->is_readonly ))
    return false;

  lock_page_frame (page);

  if (page->supplementary_frame_in_memory != NULL)
    return true;

  if (load_frame_with_data (page))
    return pagedir_set_page (thread_current ()->pagedir, page->vaddr,
        page->supplementary_frame_in_memory->base_vir_address,
        !page->is_readonly);

  return false;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/**
 * Evicts given supplementary_page_table_entry's frame from physical memory
 * either to file or swap disk
 */
bool evict_page (struct supplementary_page_table_entry* page)
{
  ASSERT(page -> supplementary_frame_in_memory != NULL)
  ASSERT(
      lock_held_by_current_thread (
          &page->supplementary_frame_in_memory->this_lock));

  struct thread *pthread = page->sup_thread;
  bool success = false;
  off_t number_of_bytes_written;
  // Clear the pagedir for page
  pagedir_clear_page (pthread->pagedir, page->vaddr);

  if (page->sup_file == NULL)
  {
    success = swap_page_out_to_disk_from_frame (page);
  } else
  {
    // If page is dirty - meaning written to - modified
    if (pagedir_is_dirty (pthread->pagedir, page->vaddr))
    {
      // Check if swap_write value if - its allowed to swap to disk
      if (page->swap_write)
      {
        success = swap_page_out_to_disk_from_frame (page);
        // If swap_write - indicates swap the page with actual file
      } else
      {
        // number of bytes actually written
        number_of_bytes_written = file_write_at (page->sup_file,
            page->supplementary_frame_in_memory->base_vir_address,
            page->rw_bytes, page->file_offset);
        // If bytes written were equals to actual bytes in the frame, then return true
        if (number_of_bytes_written == page->rw_bytes)
        {
          page->supplementary_frame_in_memory = NULL;
          return true;
        } else
        {
          return false;
        }
      }
    } else
    {
      page->supplementary_frame_in_memory = NULL;
      return true;
    }
  }

  // If true, meaning eviction was successful - then release the frame pointer to NULL
  if (success)
  {
    page->supplementary_frame_in_memory = NULL;
  }

  return success;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/**
 * Deallocate the supplementary_page_table_entry for given virtual address
 */
void deallocate_supplementary_PTE (void* vir_addr)
{
  struct thread *current = thread_current ();
  struct supplementary_page_table_entry *page = get_page_table_entry (vir_addr);
  ASSERT(page != NULL);
  lock_page_frame (page);
  // If it has frame associated with it - release it too!
  if (page->supplementary_frame_in_memory)
  {
    struct supplementary_frame_in_memory *local_frame =
        page->supplementary_frame_in_memory;

    // If its in memory evict it first and then release it
    if (page->sup_file && !page->swap_write)
    {
      evict_page (page);
    }

    // Release the frame
    ASSERT(lock_held_by_current_thread (&local_frame->this_lock));
    local_frame->process_pg = NULL;
    lock_release (&local_frame->this_lock);
  }
  hash_delete (current->supplementary_page_table, &page->elem);
  free (page);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* Lock the frame associated with supplementary_page_table_entry */
void lock_page_frame (struct supplementary_page_table_entry *page)
{
  struct supplementary_frame_in_memory *frame =
      page->supplementary_frame_in_memory;
  if (frame != NULL)
  {
    // Acquire the frame's lock
    lock_acquire (&frame->this_lock);
    // if frame is not associated with this page and now not in memory
    if (frame != page->supplementary_frame_in_memory)
    {
      // Release the lock
      lock_release (&frame->this_lock);
      ASSERT(page -> supplementary_frame_in_memory == NULL);
    }
  }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* Unlock the page's frame */
void unlock_frame_from_page (const void *addr)
{
  // Get the supplementary_page_table_entry using virtual address from page table
  struct supplementary_page_table_entry *page = get_page_table_entry (addr);
  ASSERT(page != NULL);
  ASSERT(
      lock_held_by_current_thread (
          &page->supplementary_frame_in_memory->this_lock));
  // Unlock the frame associated with this page
  lock_release (&page->supplementary_frame_in_memory->this_lock);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
