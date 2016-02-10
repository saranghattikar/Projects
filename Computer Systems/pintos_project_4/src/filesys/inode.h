#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

/* Inode related constants */
// 8 MB maximum file size limit - as total partition size is 8 MB
#define MAX_FILE_SIZE 8388608
// Total Inode data-block pointers
#define INODE_BLOCKS_PTRS 14
// Direct datablock pointers
#define DIRECT_BLOCK_PTRS 4
// Indirect datablock pointers
#define INDIRECT_BLOCK_PTRS 9
// Double indirect datablock pointers
#define DOUBLE_INDIRECT_BLOCK_PTRS 1
// Total number of indirect data-blocks
#define INDIRECT_BLOCKS_PTRS 128

void inode_init (void);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);

/* Newly added or Modified functions */
bool is_inode_dir (const struct inode *inode);
off_t inode_length (const struct inode *inode);
int get_inode_open_cnt (const struct inode *inode);
block_sector_t get_inode_parent (const struct inode *inode);
bool inode_add_parent (block_sector_t parent_sector,
		       block_sector_t child_sector);
void acquire_inode_lock (const struct inode *inode);
void release_inode_lock (const struct inode *inode);
bool inode_create (block_sector_t sector, off_t length, bool isdir);


#endif /* filesys/inode.h */
