#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h" 

struct lock io_lock;

void syscall_init (void);
void validate_vaddr(void*, int);
void handle_exit(int);
void validate_sp_with_argnum(void*, int);
void validate_buffer(void* ptr, int size, int writable);
void validate_fd(int, int);
#endif /* userprog/syscall.h */
