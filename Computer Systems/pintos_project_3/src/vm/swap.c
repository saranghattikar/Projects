#include <bitmap.h>
#include <debug.h>
#include <stdio.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

// Page-swap related functions
/**
 *Initialize the page swapping related data-structures
 */
void initialize_swap_table ()
{
  swap_partition = block_get_role (BLOCK_SWAP);
  lock_init (&swap_lock);
  // Check if no swap partition is available then swapping is disabled
  if (swap_partition == NULL)
  {
    // meaning that swapping is disabled but programs should still run
    // until there is enough physical memory avaiable
    swap_sectors_btmp = bitmap_create (0);
  } else
  {
    // Create bitmap for total number of page swap sectors
    // (i.e. number of sectors rounded to per-page sectors) in swap partition
    // Total number of swap sectors = (total block-size of swap partition)
    //                                /(per page swap sectors)
    swap_sectors_btmp = bitmap_create (
        block_size (swap_partition) / NUM_OF_PER_PAGE_SECTORS);
  }
  // If unable to create swap sectors bitmap, meaning kernel memory is full - PANIL :(
  if (swap_sectors_btmp == NULL)
  {
    PANIC("Swap Sectors bitmap creation failed!!! :(");
  }
  // Reset all bits in this bitmap - to zero
  bitmap_set_all (swap_sectors_btmp, 0);
}

/**
 * This function will cause - given page to reinstate from swap sectors to frame
 */
void swap_page_in_from_disk_to_frame (struct supplementary_page_table_entry * page)
{
  int i = 0;
  if (swap_sectors_btmp == NULL)
  {
    PANIC("NO swap sectors bitmap available!!! :(");
  } else if (bitmap_test (swap_sectors_btmp, page->area_swap) == false)
  {
    PANIC("Reading from non empty swap sectors!!! :(");
  }

  for (i = 0; i < NUM_OF_PER_PAGE_SECTORS; i++)
  {
    block_read (swap_partition,
        ( page->area_swap * NUM_OF_PER_PAGE_SECTORS ) + i,
        // block_read (swap_partition, page->area_swap + i,
        page->supplementary_frame_in_memory-> base_vir_address + i * BLOCK_SECTOR_SIZE);
  }

  // Reset the sector bit in swap_sector_bitmap to 0 - indicate
  // its free now and can be used by other page-swaps
  bitmap_set (swap_sectors_btmp, page->area_swap, false);

  // Set the Index of a block device sector to maximum value,
  // BITMAP_ERROR - indicating its not swapped
  page->area_swap = (block_sector_t) -1;
}

/**
 * function swaps the given page frame to the swap_disk_partition
 */
bool swap_page_out_to_disk_from_frame (struct supplementary_page_table_entry *page)
{
  int i = 0;
  size_t open_index;
  ASSERT (page -> supplementary_frame_in_memory != NULL);
  ASSERT (lock_held_by_current_thread (&page -> supplementary_frame_in_memory ->this_lock));

  if (swap_sectors_btmp == NULL)
  {
    PANIC("NO swap sectors bitmap available!!! :(");
  }

  // Get any free empty swap-sector index - to which we will write our page-frame data
  lock_acquire (&swap_lock);
  open_index = bitmap_scan_and_flip (swap_sectors_btmp, 0, 1, 0);
  lock_release (&swap_lock);

  if (open_index != BITMAP_ERROR)
  {
    page->area_swap = open_index;

    for (i = 0; i < NUM_OF_PER_PAGE_SECTORS; i++)
    {
      block_write (swap_partition,
          ( page->area_swap * NUM_OF_PER_PAGE_SECTORS ) + i,
          page->supplementary_frame_in_memory-> base_vir_address + i * BLOCK_SECTOR_SIZE);
    }

    // set that sector_bit_index in bitmap to 1
    bitmap_set (swap_sectors_btmp, page->area_swap, true);

    page->sup_file = NULL;
    page->swap_write = false;
    page->rw_bytes = 0;
    page->file_offset = 0;
    return true;
  }
  return false;
}

