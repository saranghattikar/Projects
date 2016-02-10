#include <stddef.h>
#include <stdbool.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "filesys/inode.h"

/* Length of file name */
#define MAXIMUM_FILE_NAME_LENGTH 14

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[MAXIMUM_FILE_NAME_LENGTH + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };



bool dir_create (size_t entry_cnt, block_sector_t sector);
struct dir *dir_open (size_t count, struct inode *, struct dir *);
bool get_parent_directory (struct dir* dir, struct inode **inode);
void dir_close (struct dir *);
bool is_parent_directory (struct dir* dir);

bool dir_lookup (const struct dir *, struct inode **,const char *name);
bool dir_add (struct dir *, block_sector_t, const char *name);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name [MAXIMUM_FILE_NAME_LENGTH + 1]);


