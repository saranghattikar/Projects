/*
 * vmutils.h
 * Created on: Apr 1, 2015
 * Author: Shruthi, Pramod
 */
#include <hash.h>
#include "lib/stdbool.h"
#include "filesys/off_t.h"
#include "devices/block.h"

/**
 * Publicly exposed functions
 */
bool hash_values_comparator (const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux);
unsigned get_hash_value_for_page (const struct hash_elem *elem, void *aux);

struct supplementary_frame_in_memory *evict_frame (
    struct supplementary_page_table_entry *page,
    size_t present_frames,
    struct supplementary_frame_in_memory *global_frame,
    size_t frame_count);

struct supplementary_frame_in_memory *check_for_free_frame (
    struct supplementary_page_table_entry *page,
    struct supplementary_frame_in_memory *global_frame,
    size_t frame_count);
