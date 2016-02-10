#include <stdio.h>
#include <stddef.h>

#include "vm/vmutils.h"
#include "vm/frame.h"
#include "vm/page.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

#include "devices/timer.h"


struct supplementary_frame_in_memory *allot_frame_for_page_in_memory (
    struct supplementary_page_table_entry *page)
{
  int i =1;
  while(i < 4)
  {
    struct supplementary_frame_in_memory *frame ;
    lock_acquire (&frame_global_lock);
    /* Check if any frame in the memory is free */
    frame = check_for_free_frame(page, global_frame, frame_count);
    /* If no frame is free then evict it */
    if(frame == NULL)
      frame = evict_frame (page, present_frames, global_frame, frame_count);
    lock_release (&frame_global_lock);
    if (frame != NULL)
    {
      ASSERT(lock_held_by_current_thread (&frame->this_lock));
      return frame;
    }

    timer_msleep (1000);
    i++;
  }
  return NULL;
}

void initialize_supplementary_frame_in_memory ()
{
  void *base_address;
  global_frame = malloc (sizeof *global_frame * init_ram_pages);

  if (global_frame == NULL)
    PANIC("Frame allocation of pages in memory failed");

  while ((base_address = palloc_get_page (PAL_USER)) != NULL)
  {
    struct supplementary_frame_in_memory *frame =
        &global_frame [frame_count++];
    lock_init (&frame->this_lock);
    frame->base_vir_address = base_address;
    frame->process_pg = NULL;
  }
  lock_init (&frame_global_lock);
}
