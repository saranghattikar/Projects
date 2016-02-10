#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "../lib/user/syscall.h"
#include "vm/page.h"

#define STDIN_FILENO    0       /* standard input file descriptor */
#define STDOUT_FILENO   1       /* standard output file descriptor */

/* Lock for FileSystem Operations */
static struct lock file_op_lock;
static void syscall_handler (struct intr_frame *);
void close_all_opened_files (struct thread *current);
static int close_files (int fd);
int exec_process (char * file_name);

void syscall_init (void)
{
  lock_init (&file_op_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/**
 * Check if pointer value is in user-virtual memory space
 * The code segment in Pintos starts at user virtual address 0x08084000,
 * approximately 128 MB from the bottom of the address space,
 * and expands till PHYS_BASE 0xc0000000
 *
 * Ref: http://w3.cs.jmu.edu/kirkpams/450-f14/projects/process_project.shtml
 */
static void is_ptr_in_user_vm_space (const void *vaddr)
{
  if (vaddr >= PHYS_BASE || vaddr < ( (void *) 0x08048000 ))
  {
    // Exit the thread as it is trying to access invalid memory location
    exit_thread (-1);
  }
}

/**
 * Validate if each pointer in buffer is in valid User Virtual Memory Space.
 */
static void validate_buffer_pointers (const void* buffer_, unsigned size)
{
  const void *buffer = buffer_;
  is_ptr_in_user_vm_space (buffer);
  unsigned i;
  for (i = 0; i < size; i++)
  {
    is_ptr_in_user_vm_space ((const void*) buffer);
    (char *) buffer++;
  }
}

/**
 * Get given number (i.e. limit) of  arguments from stack frame,
 * and check if their pointers are in the user virtual space.
 */
static void extract_args (struct intr_frame *f, int *args, int limit)
{
  int i;
  int *ptr;
  for (i = 0; i < limit; i++)
  {
    ptr = (int *) f->esp + i + 1;
    is_ptr_in_user_vm_space ((const void *) ptr);
    args [i] = *ptr;
  }
}

/**
 * Just shut down the OS
 */
void halt (void)
{
  shutdown_power_off ();
}

/**
 *  SYS_EXIT call handler
 *  Terminates the current user program by exiting
 *  the current thread.
 */
void exit_thread (int status)
{
  struct thread *cur_thread = thread_current ();
  printf ("%s: exit(%d)\n", cur_thread->name, status);
  cur_thread->_state->exit_status = status;
  cur_thread->exit_status = status;
  thread_exit ();
}

/**
 * Wait for a particular process (given by pid) to complete.
 */
int wait (pid_t pid)
{
  return process_wait (pid);
}

/**
 * Copies given commandline argument string to kernel virtual memory page,
 * and returns its pointer
 */
char * copy_commandline_arguments_to_kernel_page (char *commandline_args)
{
  char * copy_args, *uvaddr_page;
  int i = 0;
  bool user_vm_page_lock_status = false;
  // If kernel memory page allocation fails meaning something wrong - exit the thread
  if ( ( copy_args = palloc_get_page (0) ) == NULL)
  {
    // Kill this thread
    exit_thread (-1);
  }

  // Get the page from user virtual memory address
  uvaddr_page = pg_round_down (commandline_args);
  user_vm_page_lock_status = lock_and_write_page (uvaddr_page, false);
  // Get the lock over this user-virtual memory page
  if (user_vm_page_lock_status == true)
  {
    for (i = 0; i < PGSIZE; commandline_args++, i++)
    //should be less than - commandline_args < PGSIZE + uvaddr_page
    {
      copy_args [i] = *commandline_args;
      if (*commandline_args == '\0')
      {
        // Meaning whole arguments string is copied into kernel virtual memory
        break;
      }
    }
    unlock_frame_from_page (uvaddr_page);
    return copy_args;
  } else
  {
    palloc_free_page (copy_args);
    // Kill this thread
    exit_thread (-1);
  }
  return NULL;
}

/*
 * Execute a process with a given file name.
 * Return -1 if the process did not load.
 * Return the thread's id if the new process is load.
 */
int exec_process (char * file_name)
{
  struct lock temp_lock;
  char * args = copy_commandline_arguments_to_kernel_page (file_name);
  lock_acquire (&file_op_lock);
  tid_t tid_returned = process_execute ((const char *) args);
  struct thread *t = get_alive_thread_by_tid (tid_returned);
  struct _child *child = t->_state;
  lock_init (&temp_lock);
  lock_acquire (&temp_lock);
  //if the process has not yet loaded. Wait for it.
  while (t != NULL && child->is_loaded == THREAD_YET_TO_LOAD)
  {
    cond_wait (&child->wait_for_me_to_load, &temp_lock);
  }
  lock_release (&temp_lock);
  lock_release (&file_op_lock);
  //After the process has been loaded, return.
  if (t == NULL || child->is_loaded == THREAD_LOAD_NEGATIVE)
  {
    return -1;
  } else
  {
    // If process loaded successfully then only add it to children list and assign the parent
    /*if (strcmp (t->name, "main") != 0)
     {
     t->parent = thread_current ();
     }
     list_push_front (&thread_current ()->children, &child->child_elem);*/
    return tid_returned;
  }
}

/**
 * Find the map with given id from the map
 * list of current thread and return the map structure if present
 * else null.
 */
static struct map_structure* extract_map (int id)
{
  struct thread *current = thread_current ();
  struct list_elem *map_elem;

  for (map_elem = list_begin (&current->maps);
      map_elem != list_end (&current->maps); map_elem = list_next (map_elem))
  {
    struct map_structure *map_struct_local = list_entry(map_elem,
        struct map_structure, map_elem);
    if (id == map_struct_local->id)
    {
      return map_struct_local;
    }
  }
  return NULL;
}

/**
 * Find the file with given file-descriptor from the opened_files
 * list of current thread and return the file structure if present
 * else null.
 */
static struct file_structure* get_file_structure (int fd)
{
  struct thread *current = thread_current ();
  struct list_elem *file_elem;

  for (file_elem = list_begin (&current->opened_files);
      file_elem != list_end (&current->opened_files);
      file_elem = list_next (file_elem))
  {
    struct file_structure *file_struct_local = list_entry(file_elem,
        struct file_structure, file_elem);
    if (fd == file_struct_local->fd)
    {
      return file_struct_local;
    }
  }
  return NULL;
}

size_t get_size_to_be_read_or_written (uint8_t *buffer, unsigned file_length,
bool read)
{
  if (!lock_and_write_page (buffer, read))
    exit_thread (-1);

  size_t size_of_the_page_left;
  size_of_the_page_left = PGSIZE - pg_ofs (buffer);

  return
      file_length < size_of_the_page_left ? file_length : size_of_the_page_left;

}
off_t get_initial_bytes_written (struct file_structure *file_struct,
    unsigned size_to_be_written, uint32_t buffer_to_be_written)
{
  lock_acquire (&file_op_lock);
  off_t return_bytes;

  if (file_struct != NULL)
  {
    return_bytes = file_write (file_struct->file, buffer_to_be_written,
        size_to_be_written);
  } else
  {
    putbuf ((char *) buffer_to_be_written, size_to_be_written);
    return_bytes = size_to_be_written;
  }

  lock_release (&file_op_lock);
  unlock_frame_from_page (buffer_to_be_written);

  return return_bytes;

}

/**
 * Writes given number of bytes (i.e. size) from buffer to the open file fd.
 * Returns 0 if size invalid, return size if fd is 1 i.e. STDOUT_FILENO and
 * the number of bytes actually written if written to a file.
 */
static int write_file (struct file_structure *file_struct, const void *buffer,
    unsigned file_length)
{
  uint8_t *buffer_to_be_written = buffer;
  int final_no_of_bytes_written = 0;

  while (file_length > 0)
  {
    size_t size_to_be_written = get_size_to_be_read_or_written (
        buffer_to_be_written, file_length, false);

    off_t initial_bytes_written = get_initial_bytes_written (file_struct,
        size_to_be_written, buffer_to_be_written);

    if (initial_bytes_written < 0)
    {
      if (final_no_of_bytes_written == 0)
        final_no_of_bytes_written = -1;
      break;
    }

    final_no_of_bytes_written += initial_bytes_written;

    if ((off_t) size_to_be_written != initial_bytes_written)
      break;

    buffer_to_be_written += initial_bytes_written;
    file_length -= initial_bytes_written;
  }

  return final_no_of_bytes_written;

}

/**
 * Read bytes into buffer from file with given file-descriptor.
 * Returns 0 if size is less than 1 else number of bytes
 * read.
 */
static int read_file (struct file_structure *file_struct, void *buffer,
    unsigned file_length)
{
  uint8_t *buffer_to_be_read = buffer;
  int final_no_of_bytes_read = 0;
  size_t counter;

  while (file_length > 0)
  {
    size_t size_to_be_read = get_size_to_be_read_or_written (buffer_to_be_read,
        file_length, true);
    off_t initial_bytes_read;

    if (file_struct->fd == STDIN_FILENO)
    {
      final_no_of_bytes_read = size_to_be_read;
      counter = 0;
      while (counter < size_to_be_read)
      {
        buffer_to_be_read [counter] = (char) input_getc ();
        counter++;
      }
    } else
    {
      lock_acquire (&file_op_lock);
      initial_bytes_read = file_read (file_struct->file, buffer_to_be_read,
          size_to_be_read);
      lock_release (&file_op_lock);
    }

    unlock_frame_from_page (buffer_to_be_read);
    if (initial_bytes_read < 0)
    {
      if (final_no_of_bytes_read == 0)
        final_no_of_bytes_read = -1;
      break;
    }

    final_no_of_bytes_read += initial_bytes_read;

    if ((off_t) size_to_be_read != initial_bytes_read)
      break;

    file_length -= initial_bytes_read;
    buffer_to_be_read += initial_bytes_read;
  }
  return final_no_of_bytes_read;
}

/**
 * Open the file with given file name.
 * Returns -1 if file not present else the
 * file descriptor of opened file.
 */
static int open_file (const char *file_name)
{
  lock_acquire (&file_op_lock);
  struct file *file = filesys_open (file_name);
  if (!file)
  {
    lock_release (&file_op_lock);
    return -1;
  }
  struct thread *current = thread_current ();
  // struct file_structure *file_struct = palloc_get_page (PAL_ZERO);
  struct file_structure *file_struct = (struct file_structure *) malloc (
      sizeof(struct file_structure));
  file_struct->fd = current->thread_fd;
  file_struct->file = file;
  list_push_back (&current->opened_files, &file_struct->file_elem);
  current->thread_fd++;
  lock_release (&file_op_lock);
  return file_struct->fd;
}

/**
 * Close all opened files by this thread, this is done when thread is dying
 */
void close_all_opened_files (struct thread *current)
{
  struct list_elem *file_elem;

  for (file_elem = list_begin (&current->opened_files);
      file_elem != list_end (&current->opened_files);
      file_elem = list_next (file_elem))
  {
    struct file_structure *f = list_entry(file_elem, struct file_structure,
        file_elem);
    // Close the file
    lock_acquire (&file_op_lock);
    file_close (f->file);
    f->file = NULL;
    lock_release (&file_op_lock);
    list_remove (&f->file_elem);
    // free (f);
    // palloc_free_page(f);
    f = NULL;
  }
}

/**
 * Close all mapped files by this thread, this is done when thread is dying
 */
void close_all_mapped_files (struct thread *t)
{
  struct list_elem *e;
// close and unmap all the memory mapped files
  for (e = list_begin (&t->maps); e != list_end (&t->maps); e = list_next (e))
  {
    struct map_structure *map = list_entry(e, struct map_structure, map_elem);
    size_t i;
    void * start_addr = (void *) map->address;
    for (i = 0; i < map->pages_count; i++)
    {
      deallocate_supplementary_PTE (start_addr);
      start_addr += PGSIZE;
    }
    lock_acquire (&file_op_lock);
    file_close (map->map_file);
    map->map_file = NULL;
    lock_release (&file_op_lock);
    list_remove (&map->map_elem);
    //free (map);
  }
}
/**
 * Calculates file size for the file with the given file-descriptor.
 */
int filesize (int fd)
{
  struct file_structure *file_struct = get_file_structure (fd);
  if (!file_struct)
  {
    return -1;
  }
  lock_acquire (&file_op_lock);
  int size = (int) file_length (file_struct->file);
  lock_release (&file_op_lock);
  file_struct = NULL;
  return size;
}

/**
 * Sets the file-pointer to given position of the file with the given file-descriptor.
 */
static int fileseek (int fd, unsigned ps)
{
  struct file_structure *file_struct = get_file_structure (fd);
  int status = 0;
  if (!file_struct)
  {
    return -1;
  }
  lock_acquire (&file_op_lock);
  file_seek (file_struct->file, ps);
  lock_release (&file_op_lock);
  file_struct = NULL;
  return status;
}

/**
 * Returns the current position of the pointer in the file having the given descriptor,
 * else returns -1 if file not present.
 */
static int filetell (int fd)
{
  unsigned tell_pointer;
  struct file_structure *file_struct = get_file_structure (fd);
  if (!file_struct)
  {
    return -1;
  }
  lock_acquire (&file_op_lock);
  tell_pointer = (unsigned) file_tell (file_struct->file);
  lock_release (&file_op_lock);
  file_struct = NULL;
  return tell_pointer;
}

/**
 * Unmap all pages and their frames and free the structures as well
 */
void unmap_memory_pages_and_frames (struct map_structure *map)
{
  size_t i;
  void * start_addr = (void *) map->address;
  struct file *map_file = map->map_file;
  for (i = 0; i < map->pages_count; i++)
  {
    deallocate_supplementary_PTE (start_addr);
    start_addr += PGSIZE;
  }
  lock_acquire (&file_op_lock);
  file_close (map_file);
  map_file = NULL;
  lock_release (&file_op_lock);
  list_remove (&map->map_elem);
  free (map);
}
/**
 * Closes the file with given file-descriptor
 */
static int close_files (int fd)
{
  struct file_structure *file_struct = get_file_structure (fd);
  int status = 0;
  if (file_struct != NULL)
  {
    lock_acquire (&file_op_lock);
    file_close (file_struct->file);
    file_struct->file = NULL;
    lock_release (&file_op_lock);
    list_remove (&file_struct->file_elem);
    free (file_struct);
    file_struct = NULL;
  }
  return status;
}

bool file_present_in_map (struct map_structure *local_map, void * addr,
    struct file *file)
{
  struct thread *current = thread_current ();

  local_map->id = current->thread_fd;
  current->thread_fd++;

  lock_acquire (&file_op_lock);
  local_map->map_file = file_reopen (file);
  lock_release (&file_op_lock);

  if (local_map->map_file == NULL)
  {
    free (local_map);
    return false;
  }

  local_map->address = addr;
  local_map->pages_count = 0;
  list_push_front (&current->maps, &local_map->map_elem);
  return true;
}

off_t get_map_file_size (struct file *file)
{
  off_t size;
  lock_acquire (&file_op_lock);
  size = file_length (file);
  lock_release (&file_op_lock);
  return size;
}
/*
 * Maps the memory
 */
static int memory_map (int fd, void *addr)
{
  size_t page_offset_address = 0;
  off_t file_size;
  struct file_structure *file_struct = get_file_structure (fd);
  struct map_structure *local_map = malloc (sizeof *local_map);

  if (!file_struct || addr == NULL || pg_ofs (addr) != 0 || local_map == NULL)
    return -1;

  if (!file_present_in_map (local_map, addr, file_struct->file))
    return -1;

  file_size = get_map_file_size (local_map->map_file);

  if (file_size < 0)
    return -1;

  while (file_size > 0)
  {
    struct supplementary_page_table_entry *page = allocate_supplementary_PTE (
        page_offset_address + (uint8_t *) addr, true);
    if (page == NULL)
    {
      unmap_memory_pages_and_frames (local_map);
      return -1;
    }

    local_map->pages_count += 1;

    page->rw_bytes = file_size < PGSIZE ? file_size : PGSIZE;

    page->file_offset = page_offset_address;
    page->swap_write = false;
    page->sup_file = local_map->map_file;
    file_size = file_size - page->rw_bytes;
    page_offset_address = page_offset_address + page->rw_bytes;
  }
  return local_map->id;
}

/**
 * Method to handle all the system calls.
 */
static void syscall_handler (struct intr_frame *f UNUSED)
{
  int args [3];
  is_ptr_in_user_vm_space ((const void*) f->esp);
  switch (*(int *) f->esp)
  {
  case SYS_HALT:
  {
    halt ();
    break;
  }
  case SYS_EXIT:
  {
    extract_args (f, &args [0], 1);
    exit_thread (args [0]);
    break;
  }
  case SYS_EXEC:
  {
    extract_args (f, &args [0], 1);
    f->eax = exec_process ((const char *) args [0]);
    break;
  }
  case SYS_WAIT:
  {
    extract_args (f, &args [0], 1);
    f->eax = wait (args [0]);
    break;
  }
  case SYS_CREATE:
  {
    char *filename = NULL;
    extract_args (f, &args [0], 2);
    filename = copy_commandline_arguments_to_kernel_page ((char *) args [0]);
    lock_acquire (&file_op_lock);
    f->eax = filesys_create (filename, args [1]);
    lock_release (&file_op_lock);
    palloc_free_page (filename);
    break;
  }
  case SYS_REMOVE:
  {
    char *filename = NULL;
    extract_args (f, &args [0], 1);
    filename = copy_commandline_arguments_to_kernel_page ((char *) args [0]);
    lock_acquire (&file_op_lock);
    f->eax = filesys_remove ((const char *) filename);
    lock_release (&file_op_lock);
    palloc_free_page (filename);
    break;
  }
  case SYS_OPEN:
  {
    char *filename = NULL;
    extract_args (f, &args [0], 1);
    filename = copy_commandline_arguments_to_kernel_page ((char *) args [0]);
    f->eax = open_file ((const char *) filename);
    palloc_free_page (filename);
    break;
  }
  case SYS_FILESIZE:
  {
    extract_args (f, &args [0], 1);
    f->eax = filesize (args [0]);
    break;
  }
  case SYS_READ:
  {
    extract_args (f, &args [0], 3);
    validate_buffer_pointers ((const void *) args [1], (unsigned) args [2]);
    struct file_structure *file_struct = get_file_structure (args [0]);
    if (file_struct == NULL)
    {
      f->eax = -1;
      break;
    }
    f->eax = (uint32_t) read_file (file_struct, (void *) args [1],
        (unsigned) args [2]);
    break;
  }
  case SYS_WRITE:
  {
    extract_args (f, &args [0], 3);
    validate_buffer_pointers ((const void *) args [1], (unsigned) args [2]);
    struct file_structure *file_struct;
    if (args [0] != 1)
    {
      file_struct = get_file_structure (args [0]);
      if (file_struct == NULL)
      {
        f->eax = -1;
        break;
      }
    } else
      file_struct = NULL;

    f->eax = (uint32_t) write_file (file_struct, (const void *) args [1],
        (unsigned) args [2]);
    break;
  }
  case SYS_SEEK:
  {
    extract_args (f, &args [0], 2);
    f->eax = (uint32_t) fileseek (args [0], (unsigned) args [1]);
    break;
  }
  case SYS_TELL:
  {
    extract_args (f, &args [0], 1);
    f->eax = (uint32_t) filetell (args [0]);
    break;
  }
  case SYS_CLOSE:
  {
    extract_args (f, &args [0], 1);
    f->eax = close_files (args [0]);
    break;
  }
  case SYS_MMAP:
  {
    extract_args (f, &args [0], 2);
    f->eax = memory_map (args [0], (void *) args [1]);
    break;
  }
  case SYS_MUNMAP:
  {
    extract_args (f, &args [0], 1);
    struct map_structure *map = extract_map (args [0]);
    if (map != NULL)
    {
      unmap_memory_pages_and_frames (map);
    }
    break;
  }
  }
}
