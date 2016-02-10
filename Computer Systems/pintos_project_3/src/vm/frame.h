#include "threads/synch.h"


/* Structure for a supplement frame in memory */
struct supplementary_frame_in_memory
{
  struct supplementary_page_table_entry *process_pg;     /* Points to the page of process */
  void *base_vir_address;         /* Gives the base pointer of the virtual address of kernel */
  struct lock this_lock;          /* Lock variable for this frame */
  };

/* Global variables for frames */
static struct lock frame_global_lock;
static size_t present_frames;
static struct supplementary_frame_in_memory *global_frame;
static size_t frame_count;

/* Functions in frame */
void initialize_frame ();
struct supplementary_frame_in_memory *allot_frame_for_page_in_memory(struct supplementary_page_table_entry *page);

