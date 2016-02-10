#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>

#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#include "vm/page.h"
#include "vm/frame.h"
#include "vm/vmutils.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
// Method for pushing process arguments onto the thread_stack
static bool push_args (void **esp, char* commandline_args,
    uint8_t *user_page_base_addr, uint8_t *kernel_page_base_addr);
// Validation method to check if stack has overflowed its page size

/**
 * Free up the thread structure, close and release file handles, remove and
 * free-up the children _child structures as well
 */
void release_thread_resources (struct thread *t)
{
  struct list_elem *e;

  /* Close all mapped files */
  close_all_mapped_files (t);

  /* Close any opened files in this thread */
  close_all_opened_files (t);

  /* All its children threads now become orphan threads i.e. without any parent */
  for (e = list_begin (&t->children); e != list_end (&t->children); e =
      list_next (e))
  {
    struct _child *child = list_entry(e, struct _child, child_elem);
    if (child->pid != 2)
    {
      list_remove (&child->child_elem);
      if (!child->is_thread_exited)
      {
        child->thread_ptr->parent = NULL;
      } else
      {
        child->thread_ptr = NULL;
        //free (child);
      }
    }
  }
}

/**
 * This function takes a single supplementary page table entry and releases its frame
 * and frees its structure
 */
static void clean_page_and_frames (struct hash_elem *p1, void* aux_entry UNUSED)
{
  struct supplementary_page_table_entry *page = hash_entry(p1,
      struct supplementary_page_table_entry, elem);
  lock_page_frame (page);
  struct supplementary_frame_in_memory *local_frame =
      page->supplementary_frame_in_memory;
  if (local_frame)
  {
    //free_frame (page->supplementary_frame_in_memory);
    ASSERT(lock_held_by_current_thread (&local_frame->this_lock));
    local_frame->process_pg = NULL;
    lock_release (&local_frame->this_lock);
  }
  free (page);
}

/**
 * Delete the supplementary page table from current dying thread
 */
void delete_pgtable_entry (void)
{
  struct thread *current = thread_current ();
  if (current->supplementary_page_table != NULL)
    hash_destroy (current->supplementary_page_table, clean_page_and_frames);
}

/* Starts a new thread running a user program loaded from
 FILENAME.  The new thread may be scheduled (and may even exit)
 before process_execute() returns.  Returns the new process's
 thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute (const char *file_name)
{
  char *fn_copy, *save_ptr, *thread_name;
  tid_t tid;

  /* Make a copy of FILE_NAME.
   Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  // Copy just a filename into a separate filename char-array,
  // so that there is no race-condition
  char file_name_ [17];
  // First separate the filename
  strlcpy (file_name_, file_name, 17);
  thread_name = strtok_r (file_name_, " ", &save_ptr);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create ((const char*) thread_name, PRI_DEFAULT, start_process,
      fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  return tid;
}

/* A thread function that loads a user process and starts it
 running. */
static void start_process (void *file_name)
{
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  // Used a temporary lock to signal if thread-loading is successful or not,
  // this is required for EXEC syscall
  struct lock temp_lock;
  lock_init (&temp_lock);
  lock_acquire (&temp_lock);
  success = load (file_name, &if_.eip, &if_.esp);
  struct _child *child = thread_current ()->_state;
  if (!success)
  {
    child->is_loaded = THREAD_LOAD_NEGATIVE;
  } else
  {
    child->is_loaded = THREAD_LOAD_POSITIVE;
  }
  // Signal that process is laoded successfully
  cond_signal (&child->wait_for_me_to_load, &temp_lock);
  lock_release (&temp_lock);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success)
  {
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
   interrupt, implemented by intr_exit (in
   threads/intr-stubs.S).  Because intr_exit takes all of its
   arguments on the stack in the form of a `struct intr_frame',
   we just point the stack pointer (%esp) to our stack frame
   and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ()
  ;
}

/* Waits for thread TID to die and returns its exit status.  If
 it was terminated by the kernel (i.e. killed due to an
 exception), returns -1.  If TID is invalid or if it was not a
 child of the calling process, or if process_wait() has already
 been successfully called for the given TID, returns -1
 immediately, without waiting.

 This function will be implemented in problem 2-2.  For now, it
 does nothing. */
int process_wait (tid_t child_tid UNUSED)
{
  struct _child *child = get_child_thread_by_tid (thread_current (), child_tid);
  // If parent is already waiting or given pid is not a child-thread
  // of current thread then return -1
  if (child == NULL || child->is_parent_waiting)
  {
    return -1;
  }
  child->is_parent_waiting = true;
  struct lock temp_lock;
  lock_init (&temp_lock);
  lock_acquire (&temp_lock);
  //Wait on a condition variable  using a local lock until child thread is dead
  while (!child->is_thread_exited)
  {
    cond_wait (&child->wait_for_me_to_exit, &temp_lock);
  }
  int status = child->exit_status;
  lock_release (&temp_lock);
  // Return the child's exit status
  return status;
}

/* Free the current process's resources. */
void process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  struct lock temp_lock;
  lock_init (&temp_lock);
  struct list_elem *e;

  /* Signal parent process about its exit */
  if (cur->parent != NULL)
  {
    struct _child *child = cur->_state;
    child->is_thread_exited = true;
    child->exit_status = cur->exit_status;
    lock_acquire (&temp_lock);
    cond_signal (&child->wait_for_me_to_exit, &temp_lock);
    lock_release (&temp_lock);
    cur->parent = NULL;
    child = NULL;
  } else
  {
    cur->_state->thread_ptr = NULL;
    free (cur->_state);
  }

  /* Close the executable for this process */
  file_close (cur->exe_file);

  // Release resources held by this dying thread
  release_thread_resources (cur);

  /*Delete the page table entries of the exiting process*/
  delete_pgtable_entry ();

  /* Destroy the current process's page directory and switch back
   to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
  {
    /* Correct ordering here is crucial.  We must set
     cur->pagedir to NULL before switching page directories,
     so that a timer interrupt can't switch back to the
     process page directory.  We must activate the base page
     directory before destroying the process's page
     directory, or our active page directory will be one
     that's been freed (and cleared). */
    cur->pagedir = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  }
}

/* Sets up the CPU for running user code in the current
 thread.
 This function is called on every context switch. */
void process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
   interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
 from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
  unsigned char e_ident [16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
 There are e_phnum of these, starting at file offset e_phoff
 (see [ELF1] 1-6). */
struct Elf32_Phdr
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, struct supplementary_page_table_entry *vpage);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes,
    bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 Stores the executable's entry point into *EIP
 and its initial stack pointer into *ESP.
 Returns true if successful, false otherwise. */
bool load (const char *file_name_args, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  char file_name_ [17];
  char *save_ptr, *file_name;

  /* Make a copy of file_name and argument string */
  char *file_name_args_cpy = palloc_get_page (0);
  if (file_name_args_cpy == NULL)
    return TID_ERROR;
  strlcpy (file_name_args_cpy, file_name_args, PGSIZE);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /*Allocate memory for the page table*/
  t->supplementary_page_table = malloc (sizeof *t->supplementary_page_table);

  if (t->supplementary_page_table == NULL)
  {
    goto done;
  }

  /*Create and Initialize the hash table*/
  hash_init (t->supplementary_page_table,
      get_hash_value_for_page,
      hash_values_comparator,
      NULL);

  // First separate the filename
  strlcpy (file_name_, file_name_args, 17);
  file_name = strtok_r (file_name_, " ", &save_ptr);

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
  {
    printf ("load: %s: open failed\n", file_name);
    goto done;
  }

  // save the opened executable file handle so that we can close it while exiting the thread
  t->exe_file = file;
  // Deny writes to the executable file until process finishes
  file_deny_write (file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2
      || ehdr.e_machine != 3 || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024)
  {
    printf ("load: %s: error loading executable\n", file_name);
    goto done;
  }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
  {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length (file))
      goto done;
    file_seek (file, file_ofs);

    if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type)
    {
      case PT_NULL:
      case PT_NOTE:
      case PT_PHDR:
      case PT_STACK:
      default:
        /* Ignore this segment. */
        break;
      case PT_DYNAMIC:
      case PT_INTERP:
      case PT_SHLIB:
        goto done;
      case PT_LOAD:
        if (validate_segment (&phdr, file))
        {
          bool writable = ( phdr.p_flags & PF_W ) != 0;
          uint32_t file_page = phdr.p_offset & ~PGMASK;
          uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
          uint32_t page_offset = phdr.p_vaddr & PGMASK;
          uint32_t read_bytes, zero_bytes;
          if (phdr.p_filesz > 0)
          {
            /* Normal segment.
             Read initial part from disk and zero the rest. */
            read_bytes = page_offset + phdr.p_filesz;
            zero_bytes = ( ROUND_UP(page_offset + phdr.p_memsz, PGSIZE)
                - read_bytes );
          } else
          {
            /* Entirely zero.
             Don't read anything from disk. */
            read_bytes = 0;
            zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
          }
          if (!load_segment (file, file_page, (void *) mem_page, read_bytes,
              zero_bytes, writable))
            goto done;
        } else
          goto done;
        break;
    }
  }

  /* Allocate a supplementary virtual page in Virtual Memory */
  // For stack - which grows downwards - get the last page in virtual memory
  // i.e. 4K just before PHYS_BASE virtual address, and this virtual page is writable
  struct supplementary_page_table_entry *vpage = allocate_supplementary_PTE (
      ( (uint8_t *) PHYS_BASE ) - PGSIZE, true);

  /* Set up stack. */
  if (!setup_stack (esp, vpage))
    goto done;

  // Push args on stack - here its important to maintain virtual addresses
  // as per above virtual page
  success = push_args (esp, file_name_args_cpy, vpage->vaddr,
      vpage->supplementary_frame_in_memory->base_vir_address);

  // Unlock the vitual page's physical-frame, but make sure we have its lock first
  ASSERT(
      lock_held_by_current_thread (
          &vpage->supplementary_frame_in_memory->this_lock));
  lock_release (&vpage->supplementary_frame_in_memory->this_lock);

  // If unable to push args onto stack successfully then something is wrong, exit
  if (!success)
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

  // Free-up resources
  palloc_free_page (file_name_args_cpy);
  return success;

  done:
  {
    /* We arrive here whether the load is successful or not. */
    file_close (file);
    palloc_free_page (file_name_args_cpy);
  }
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
 FILE and returns true if so, false otherwise. */
static bool validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ( ( phdr->p_offset & PGMASK ) != ( phdr->p_vaddr & PGMASK ))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
   user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) ( phdr->p_vaddr + phdr->p_memsz )))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
   address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
   Not only is it a bad idea to map page 0, but if we allowed
   it then user code that passed a null pointer to system calls
   could quite likely panic the kernel by way of null pointer
   assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 memory are initialized, as follows:

 - READ_BYTES bytes at UPAGE must be read from FILE
 starting at offset OFS.

 - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

 The pages initialized by this function must be writable by the
 user process if WRITABLE is true, read-only otherwise.

 Return true if successful, false if a memory allocation error
 or disk read error occurs. */
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs (upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    file_seek (file, ofs);
    while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Allocate a black new supplement virtual memory page*/
      // We make use of lazy loading of paging - meaning during loading process
      // load we just create the virtual memory pages and defer the actual
      // physical memory frame until actually process starts running i.e. scheduled
      struct supplementary_page_table_entry *vpage = allocate_supplementary_PTE (upage, writable);

      // Can't allocate the virtual page - something wrong - shutdown the process
      if (vpage == NULL)
        return false;

      if (page_read_bytes > 0)
      {
        vpage->sup_file = file;
        vpage->file_offset = ofs;
        vpage->rw_bytes = page_read_bytes;
      }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;

      //Update the offset for next virtual page
      ofs += page_read_bytes;
      upage += PGSIZE;
    }
    return true;
  }
}

/*
 * Pushing program arguments onto the stack,
 * Key here is - push actual values of arguments onto stack first and then push
 * their individual stack addresses (which are in the same stack) again onto the
 * same stack followed by argc count and fake return address 0
 *
 * Referred -http://www.ccs.neu.edu/home/ntuck/courses/S15/cs5600/pintos/pintos_3.html#SEC51
 */
static bool push_args (void **esp, char* commandline_args,
    uint8_t *user_page_base_addr, uint8_t *kernel_page_base_addr)
{
  int count = 0, argc = 0, word_align = 0;
  char *save_ptr, *token, *temp, *arg_align = 0;

  void *memory_address = kernel_page_base_addr + PGSIZE;
  void *bottom_of_stack = kernel_page_base_addr;

  /*
   * Make a copy of file_name and argument string, so in order to
   * first get the count of arguments, and then parse the actual
   * arguments
   */
  char *cmdln_args_cpy = palloc_get_page (0);
  if (cmdln_args_cpy == NULL)
    return false;
  strlcpy (cmdln_args_cpy, commandline_args, PGSIZE);

  // Get total number of command-line arguments
  for (token = strtok_r (commandline_args, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr))
  {
    argc++;
  }

  char *argv [argc];
  char *address [argc];
  // To store values of command line arguments into argv[]
  for (token = strtok_r (cmdln_args_cpy, " ", &save_ptr);
      count < argc && token != NULL; token = strtok_r (NULL, " ", &save_ptr))
  {
    argv [count++] = token;
  }

  //push actual argument values in right-to-left order
  for (count = argc - 1; count >= 0; count--)
  {
    int length = strlen (argv [count]) + 1;
    *esp = ( *esp - length );
    //check if memory_address is within the stack limit.
    if ((memory_address - bottom_of_stack) < ROUND_UP(length, sizeof(uint32_t)))
    {
      return false;
    }
    memory_address = (memory_address - length);
    address [count] = *esp;
    // address [count] = memory_address;

    memcpy (memory_address, argv [count], length);
  }

  //push word-alignment value required by C
  *esp -= 4;
  if ( ( memory_address - bottom_of_stack ) < ROUND_UP(4, sizeof(uint32_t)))
  {
    return false;
  }
  memory_address = ( memory_address - 4 );
  memcpy (memory_address, &word_align, 4);

  // Save the NULL character to indicate end of args
  *esp -= 1;
  //check if esp is within the stack limit.
  if ( ( memory_address - bottom_of_stack ) < ROUND_UP(1, sizeof(uint32_t)))
  {
    return false;
  }
  memory_address = ( memory_address - 1 );
  memcpy (memory_address, &arg_align, 1);

  // Now save the actual addresses onto the stack
  for (count = argc - 1; count >= 0; count--)
  {
    *esp -= sizeof(char *);
    //check if esp is within the stack limit.
    if ( ( memory_address - bottom_of_stack )
        < ROUND_UP(sizeof(char *), sizeof(uint32_t)))
    {
      return false;
    }
    memory_address = ( memory_address - sizeof(char *) );
    memcpy (memory_address, &address [count], sizeof(char *));
  }

  //push argv address
  temp = *esp;
  *esp -= sizeof(char **);
  //check if esp is within the stack limit.
  if ( ( memory_address - bottom_of_stack )
      < ROUND_UP(sizeof(char **), sizeof(uint32_t)))
  {
    return false;
  }
  memory_address = ( memory_address - sizeof(char **) );
  memcpy (memory_address, &temp, sizeof(char **));

  //push argc count value
  *esp -= sizeof(int);
  //check if esp is within the stack limit.
  if ( ( memory_address - bottom_of_stack )
      < ROUND_UP(sizeof(int), sizeof(uint32_t)))
  {
    return false;
  }
  memory_address = ( memory_address - sizeof(int) );
  memcpy (memory_address, &argc, sizeof(int));

  //push fake return address
  *esp -= sizeof(void *);
  //check if esp is within the stack limit.
  if ( ( memory_address - bottom_of_stack )
      < ROUND_UP(sizeof(void *), sizeof(uint32_t)))
  {
    return false;
  }
  memory_address = ( memory_address - sizeof(void *) );
  memcpy (memory_address, &word_align, sizeof(void *));

  // free the newly copied cmdln_args_cpy
  palloc_free_page (cmdln_args_cpy);
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
 user virtual memory. */
static bool setup_stack (void **esp, struct supplementary_page_table_entry *vpage)
{
  if (vpage == NULL)
  {
    return false;
  }
  // Allocates a physical frame for this last virtual memory page i.e. as stack
  // grows down from PHYS_BASE
  vpage->supplementary_frame_in_memory = allot_frame_for_page_in_memory (vpage);
  if (vpage->supplementary_frame_in_memory == NULL)
  {
    return false;
  }
  *esp = PHYS_BASE;
  vpage->is_readonly = false;
  vpage->swap_write = false;
  return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 virtual address KPAGE to the page table.
 If WRITABLE is true, the user process may modify the page;
 otherwise, it is read-only.
 UPAGE must not already be mapped.
 KPAGE should probably be a page obtained from the user pool
 with palloc_get_page().
 Returns true on success, false if UPAGE is already mapped or
 if memory allocation fails.
 static bool install_page (void *upage, void *kpage, bool writable)
 {
 struct thread *t = thread_current ();

 Verify that there's not already a page at that virtual
 address, then map our page there.
 return ( pagedir_get_page (t->pagedir, upage) == NULL
 && pagedir_set_page (t->pagedir, upage, kpage, writable) );
 }*/
