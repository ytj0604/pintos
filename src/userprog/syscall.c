#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  switch (*(int*)(f->esp)) {
    case SYS_HALT:
    case SYS_EXIT:
      {
        struct thread* cur = thread_current();
        if (strcmp(cur->name, "idle") && strcmp(cur->name, "main")) //Not kernel thread 
          printf ("%s: exit(%d)\n", cur->name, *(int*)(f->esp + 4));
        thread_exit();
      }
      break;
  }
}
