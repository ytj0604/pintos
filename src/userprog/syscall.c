#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"  
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//NOTE: Need synchronization for file IOs? For example, the gloabl lock for file-related APIs, applied for all processes.

static void
syscall_handler (struct intr_frame *f) 
{
  validate_vaddr(f->esp);
  switch (*(int*)(f->esp)) {
    case SYS_HALT: {
      validate_sp_with_argnum(f->esp, 0);
      shutdown_power_off();
      break;
    }
    case SYS_EXIT: {
      validate_sp_with_argnum(f->esp, 1);
      handle_exit(*(int*)(f->esp + 4));
      break;
    }
    case SYS_EXEC: {
      validate_sp_with_argnum(f->esp, 1);
      char* file = *(char**)(f->esp + 4);
      validate_vaddr(file);
      process_execute(file);
      break;
    }
    case SYS_WAIT: {
      validate_sp_with_argnum(f->esp, 1);
      int pid =  *(int*)(f->esp + 4);
      f->eax = process_wait(pid);
      break;
    }
    case SYS_CREATE: {
      validate_sp_with_argnum(f->esp, 2);
      char* file = *(char**)(f->esp + 4);
      validate_vaddr(file);
      int initial_size = *(int*)(f->esp + 8);
      f->eax = filesys_create(file, initial_size);
      break;
    }
    case SYS_REMOVE: {
      validate_sp_with_argnum(f->esp, 1);
      char* file = *(char**)(f->esp + 4);
      validate_vaddr(file);
      f->eax = filesys_remove(file);
      break;
    }
    case SYS_OPEN: {
      validate_sp_with_argnum(f->esp, 1);
      char* file = *(char**)(f->esp + 4);
      validate_vaddr(file);
      struct file *file_ptr = filesys_open(file);
      f->eax = file_ptr != NULL ? get_new_fd(file_ptr) : -1;
      break;
    }
    case SYS_FILESIZE: {
      validate_sp_with_argnum(f->esp, 1); 
      int fd = *(int*)(f->esp + 4);
      validate_fd(fd, 1);
      struct file* file_ptr = thread_current()->file_descriptor[fd];
      f->eax = file_length(file_ptr);
      break;
    }
    case SYS_READ: {
      validate_sp_with_argnum(f->esp, 3);
      int fd = *(int*)(f->esp + 4);
      validate_fd(fd, 0);
      uint8_t* buffer = *(void**)(f->esp+8);
      unsigned size = *(unsigned*)(f->esp+12);
      unsigned size_read = 0;
      validate_vaddr(buffer);

      if(fd == 0) { //read from keyboard input.
        for(unsigned i = 0; i<size; i++) {
          buffer[i] = input_getc();
          size_read++;
        }
      }
      else if(fd == 1) {
        handle_exit(-1);
      }
      else {
        struct file* file_ptr = thread_current()->file_descriptor[fd];
        size_read = file_read(file_ptr, buffer, size);
      }
      f->eax = size_read;
      break;
    }
    case SYS_WRITE:
    {
      validate_sp_with_argnum(f->esp, 1);
      int fd = *(int*)(f->esp + 4);
      validate_fd(fd, 0);
      void* buffer = *(void**)(f->esp+8);
      unsigned size = *(unsigned*)(f->esp+12);
      unsigned size_wrote = 0;
      validate_vaddr(buffer);

      if(fd == 0) handle_exit(-1);
      if(fd == 1) {
        putbuf(buffer, size);
        size_wrote = size;
      }
      else {
        struct file* file_ptr = thread_current()->file_descriptor[fd];
        size_wrote = file_write(file_ptr, buffer, size);
      }
      f->eax = size_wrote;
      break;
    }
    case SYS_SEEK: {
      validate_sp_with_argnum(f->esp, 2);
      int fd = *(int*)(f->esp + 4);
      validate_fd(fd, 1);
      unsigned position = *(unsigned*)(f->esp+8);
      struct file* file_ptr = thread_current()->file_descriptor[fd];
      file_seek(file_ptr, position);
      break;
    }
    case SYS_TELL: {
      validate_sp_with_argnum(f->esp, 1);
      int fd = *(int*)(f->esp + 4);
      validate_fd(fd, 1);
      struct file* file_ptr = thread_current()->file_descriptor[fd];
      off_t pos = file_tell(file_ptr);
      f->eax = pos;
      break;
    }
    case SYS_CLOSE: { 
      //NOTE: Multiple open for same filename generates different struct file* instance, 
      // with same inode. Closing each of them is okay. Don't close a file instance twice.
      validate_sp_with_argnum(f->esp, 1);
      int fd = *(int*)(f->esp + 4);
      validate_fd(fd, 1);
      struct file* file_ptr = thread_current()->file_descriptor[fd];
      file_close(file_ptr); //If NULL, it ignores, so okay.
      thread_current()->file_descriptor[fd] = NULL; //NULL indicates that it is closed. Don't close twice.
      break;
    }
    default:
      NOT_REACHED();
  }
}

void handle_exit(int exit_status) {
  struct thread* cur = thread_current();
  cur -> exit_status = exit_status;
  if (strcmp(cur->name, "idle") && strcmp(cur->name, "main")) //Not kernel thread 
    printf ("%s: exit(%d)\n", cur->name, exit_status);
  thread_exit();
}

void validate_vaddr(void* ptr) { //Check all 4 byte from given ptr. 
  if(ptr == NULL) handle_exit(-1);
  for(unsigned i = 0; i<4; i++) {
    if(!is_user_vaddr(ptr + i) || !pagedir_get_page(thread_current()->pagedir, ptr + i)) handle_exit(-1);
  }
}

void validate_sp_with_argnum(void* sp, int arg_cnt) { 
  // This functon gets stack ptr, and the number of arguments. And then, for each of candidate pointer of argument, validate it.
  for(int i = 0; i<(arg_cnt + 1); i++) { //Here + 1 is for syscall number.
    validate_vaddr(sp + i * 4);
  }
}

void validate_fd(int fd, int closeable) {
  // Valiate given fd.
  // fd should be: fd >= 0 && fd <= 101. fd = 0 or 1 should be checked in close, ..
  // ptr to fd should not be NULL.
  if(!(fd >= 0 && fd <= 101)) handle_exit(-1);
  if(fd == 0 || fd == 1) {
    if(closeable) handle_exit(-1);
  }
  else if(thread_current() -> file_descriptor[fd] == NULL) handle_exit(-1);
}