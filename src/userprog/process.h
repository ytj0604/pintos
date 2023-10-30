#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void parse_argument (char* file_name, int* argc, char* argv[]);
int get_new_fd(struct file* file);
#endif /* userprog/process.h */
