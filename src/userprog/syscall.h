#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void validate_vaddr(void*);
void handle_exit(int);
void validate_sp_with_argnum(void*, int);

#endif /* userprog/syscall.h */
