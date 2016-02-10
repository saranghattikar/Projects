/*
 * page.h
 * Created on: Mar 26, 2015
 * Author: Shruthi, Pramod
 */
#include <hash.h>
#include "lib/stdbool.h"
#include "filesys/off_t.h"
#include "threads/thread.h"
#include "devices/block.h"
#include "userprog/process.h"

/* Structure for a particular supplement page table entry */
struct supplementary_page_table_entry
{
  struct thread *sup_thread;      /* Thread structure associated with this page */
  struct file *sup_file;          /* File associated with this page used in memory
                                     mapping. */
  struct supplementary_frame_in_memory *supplementary_frame_in_memory;
                                  /* Frame structure associated with this page */
  struct hash_elem elem;          /* Hash element in supplementary_page_table */

  off_t file_offset;              /* Offset in the file for memory mapping*/
  off_t rw_bytes;                 /* Defines the number of bytes to read or write
                                     (Used during memory mapping). */

  block_sector_t area_swap;       /* This defines the starting sector of the swap
                                     area or -1 */

  void *vaddr;                    /* Pointer to the virtual address of the user
                                     associated with this page */

  bool is_readonly;               /* Gives true if the particular page is read only.*/
  bool swap_write;                /* If this variable is true then we need to write
                                     back to swap else to the file */
};

/**
 * Publicly exposed functions
 */
void lock_page_frame (struct supplementary_page_table_entry *page);

/* Allocate the new supplementary page table entry for given virtual address,
 * also mention if its readonly */
struct supplementary_page_table_entry *allocate_supplementary_PTE (void * vir_addr,
    bool write_only);

/* Check if this page was accessed recently using pagedir.c functions */
bool is_page_accessed (struct supplementary_page_table_entry *page);

/* Load frame with data from either file or swapped disk or zeroes */
bool load_frame_with_data (struct supplementary_page_table_entry *page);

/* Unlock the page's frame */
void unlock_frame_from_page (const void *addr);

bool check_faulted_page_for_virtual_address (void *fault_addr);

bool lock_and_write_page (const void *addr, bool write);

bool evict_page (struct supplementary_page_table_entry* page);

void deallocate_page (void* vir_addr);
