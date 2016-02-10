#include <stdbool.h>
#include <bitmap.h>

// Supplemental page structure
struct supplementary_page_table_entry;

/**
 * Swap related data-structures
 */
// Set the number of sectors per page
#define NUM_OF_PER_PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

// Common shared lock for swap-related calls
static struct lock swap_lock;

/**
 *Bitmap to store which swap partition sectors are free and occupied
 */
static struct bitmap *swap_sectors_btmp;

/**
 * structure block for swap partition.
 */
static struct block *swap_partition;

// Page-swap related functions
void initialize_swap_table ();

void swap_page_in_from_disk_to_frame (struct supplementary_page_table_entry *);

bool swap_page_out_to_disk_from_frame (struct supplementary_page_table_entry *);
