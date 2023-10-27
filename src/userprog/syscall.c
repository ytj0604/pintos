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

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

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
      break;
    }
    case SYS_REMOVE: {
      validate_sp_with_argnum(f->esp, 1);
      break;
    }
    case SYS_OPEN: {
      validate_sp_with_argnum(f->esp, 1);
      break;
    }
    case SYS_FILESIZE: {
      validate_sp_with_argnum(f->esp, 1); 
      break;
    }
    case SYS_READ: {
      validate_sp_with_argnum(f->esp, 3);
      int fd = *(int*)(f->esp + 4);
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
      else {
        printf("Not implemented yet\n");
      }
      f->eax = size_read;
      break;
    }
    case SYS_WRITE:
    {
      validate_sp_with_argnum(f->esp, 1);
      int fd = *(int*)(f->esp + 4);
      void* buffer = *(void**)(f->esp+8);
      unsigned size = *(unsigned*)(f->esp+12);
      unsigned size_wrote = 0;

      ASSERT(fd != 0);
      if(fd == 1) {
        putbuf(buffer, size);
        size_wrote = size;
      }
      else {
        printf("Write File request: Not handled yet.\n");
      }
      f->eax = size_wrote;
      break;
    }
    case SYS_SEEK: {
      validate_sp_with_argnum(f->esp, 2);
      break;
    }
    case SYS_TELL: {
      validate_sp_with_argnum(f->esp, 1);
      break;
    }
    case SYS_CLOSE: {
      validate_sp_with_argnum(f->esp, 1);
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
  for(unsigned i = 0; i<4; i++) {
    if(!is_user_vaddr(ptr + i) || !pagedir_get_page(thread_current()->pagedir, ptr + 1)) handle_exit(-1);
  }
}

void validate_sp_with_argnum(void* sp, int arg_cnt) { 
  // This functon gets stack ptr, and the number of arguments. And then, for each of candidate pointer of argument, validate it.
  for(int i = 0; i<(arg_cnt + 1); i++) { //Here + 1 is for syscall number.
    validate_vaddr(sp + i * 4);
  }
}