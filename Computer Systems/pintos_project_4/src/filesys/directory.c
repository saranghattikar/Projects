#include <string.h>
#include <stdio.h>
#include <list.h>

#include "threads/malloc.h"

#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"

/* Create a directory */
bool dir_create (size_t entry_cnt, block_sector_t sector)
{
  return inode_create (sector, entry_cnt * sizeof(struct dir_entry), true);
}

bool is_parent_directory (struct dir* directory)
{
  if (!directory)
    return false;
  if (ROOT_DIR_SECTOR == get_inumber (directory->inode))
    return true;
  return false;
}

/* Closes the directory and frees it */
void dir_close (struct dir* directory)
{
  if (directory != NULL)
  {
    inode_close (directory->inode);
    free (directory);
  }
}

/* Opens a directory */
struct dir *dir_open (size_t count, struct inode *inode,
    struct dir *directory_)
{
  struct dir *dir = calloc (1, sizeof *dir);
  //open root directory
  if (count == 0)
    inode = inode_open (ROOT_DIR_SECTOR);

  //reopen a directory
  if (count == 1)
    inode = inode_reopen (directory_->inode);

  if (dir == NULL && inode == NULL)
  {
    inode_close (inode);
    free (dir);
    return NULL;

  } else
  {
    dir->pos = 0;
    dir->inode = inode;
    return dir;
  }
}

bool get_parent_directory (struct dir* directory, struct inode **inode)
{
  *inode = inode_open (get_inode_parent (directory->inode));
  return *inode != NULL;
}

bool dir_lookup (const struct dir *directory, struct inode **inode,
    const char *file)
{
  struct dir_entry directory_element;
  size_t offset;
  ASSERT(directory != NULL);
  ASSERT(file != NULL);

  acquire_inode_lock (directory->inode);
  for (offset = 0;
      inode_read_at (directory->inode, &directory_element,
          sizeof directory_element, offset) == sizeof directory_element;
      offset += sizeof directory_element)
    if (directory_element.in_use
        && !strcmp (file, directory_element.name))
    {
      *inode = inode_open (directory_element.inode_sector);
      release_inode_lock (directory->inode);
      return *inode != NULL;
    }

  *inode = NULL;
  release_inode_lock (directory->inode);
  return *inode != NULL;
}

bool dir_add (struct dir *directory, block_sector_t block_sec,
    const char *file)
{
  struct dir_entry directory_element;
  off_t offset;
  bool success = false;

  ASSERT(directory != NULL);
  ASSERT(file != NULL);

  acquire_inode_lock (directory->inode);
  if (*file == '\0' || strlen (file) > MAXIMUM_FILE_NAME_LENGTH)
  {
    release_inode_lock (directory->inode);
    return success;
  }

  for (offset = 0;
      inode_read_at (directory->inode, &directory_element,
          sizeof directory_element, offset) == sizeof directory_element;
      offset += sizeof directory_element)
    if (directory_element.in_use
        && !strcmp (file, directory_element.name))
    {
      release_inode_lock (directory->inode);
      return success;
    }

  if (!inode_add_parent (get_inumber (directory->inode), block_sec))
  {
    release_inode_lock (directory->inode);
    return success;
  }

  for (offset = 0;
      inode_read_at (directory->inode, &directory_element,
          sizeof directory_element, offset) == sizeof directory_element;
      offset += sizeof directory_element)
    if (!directory_element.in_use)
      break;

  directory_element.in_use = true;
  strlcpy (directory_element.name, file,
      sizeof directory_element.name);
  directory_element.inode_sector = block_sec;
  success = inode_write_at (directory->inode, &directory_element,
      sizeof directory_element, offset) == sizeof directory_element;

  release_inode_lock (directory->inode);
  return success;
}

bool dir_is_empty (struct inode *inode)
{
  struct dir_entry directory_element;
  off_t offset = 0;

  for (offset = 0;
      inode_read_at (inode, &directory_element, sizeof directory_element,
          offset) == sizeof directory_element; offset +=
          sizeof directory_element)
  {
    if (directory_element.in_use)
      return false;
  }
  return true;
}

bool dir_remove (struct dir *directory, const char *file)
{
  struct dir_entry directory_element_1, directory_element_2;
  struct inode *inode = NULL;
  bool present = false;
  off_t offset_1, offset_2;
  int count;

  ASSERT(directory != NULL);
  ASSERT(file != NULL);

  acquire_inode_lock (directory->inode);

  for (offset_2 = 0;
      inode_read_at (directory->inode, &directory_element_2,
          sizeof directory_element_2, offset_2) == sizeof directory_element_2;
      offset_2 += sizeof directory_element_2)
    if (directory_element_2.in_use
        && !strcmp (file, directory_element_2.name))
    {
      directory_element_1 = directory_element_2;
      offset_1 = offset_2;
      present = true;
    }

  if (!present)
  {
    inode_close (inode);
    release_inode_lock (directory->inode);
    return false;
  }
  inode = inode_open (directory_element_1.inode_sector);
  if (inode == NULL)
  {
    inode_close (inode);
    release_inode_lock (directory->inode);
    return false;
  }
  if (is_inode_dir (inode) && get_inode_open_cnt (inode) > 1)
  {
    inode_close (inode);
    release_inode_lock (directory->inode);
    return false;
  }
  if (is_inode_dir (inode) && !dir_is_empty (inode))
  {
    inode_close (inode);
    release_inode_lock (directory->inode);
    return false;
  }
  directory_element_1.in_use = false;
  offset_2 = inode_write_at (directory->inode, &directory_element_1,
      sizeof directory_element_1, offset_1);

  if (sizeof directory_element_1 != offset_2)
  {
    inode_close (inode);
    release_inode_lock (directory->inode);
    return false;
  }
  inode_remove (inode);
  inode_close (inode);
  release_inode_lock (directory->inode);
  return true;
}

bool dir_readdir (struct dir *directory,
    char file [MAXIMUM_FILE_NAME_LENGTH + 1])
{
  struct dir_entry directory_element;
  bool success = false;
  acquire_inode_lock (directory->inode);

  while (inode_read_at (directory->inode, &directory_element,
      sizeof directory_element, directory->pos) == sizeof directory_element)
  {
    directory->pos += sizeof directory_element;
    if (directory_element.in_use)
    {
      strlcpy (file, directory_element.name,
          MAXIMUM_FILE_NAME_LENGTH + 1);
      success = true;
      break;
    }
  }
  release_inode_lock (directory->inode);
  return success;
}

