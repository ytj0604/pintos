#ifndef VM_MM_H
#define VM_MM_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include <hash.h>
#include "threads/palloc.h"

struct lock mm_lock;


struct file_mapping_entry {
    struct hash_elem file_mapping_hash_entry;
    int mapid; //For simplicity, allocate mapid to be same as given fd.
    struct file* file; //Open the file again using inode, and hold file struct.
    int size;
    void *uaddr;
};

int add_file_mapping(int fd, void* uaddr);
void delete_file_mapping(int fd, bool);
bool check_uaddr_safety(void* uaddr, int page_cnt);
void add_file_mapping_s_page_entry(void *upage, struct file* file);
unsigned file_mapping_hash_hash_func(const struct hash_elem *e, void *aux);
bool file_mapping_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
void finalize_file_mapping(void);
void file_mapping_delete_aux_func(struct hash_elem *e, void *aux UNUSED);
void delete_remove_file_mapping(int mapid);
#endif