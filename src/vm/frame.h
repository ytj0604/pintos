#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include <hash.h>
#include "threads/palloc.h"

// Hash table data structures. Global to all processes.
struct lock frame_table_lock;
struct hash frame_hash;
struct list frame_list;

struct frame_table_entry {
    struct hash_elem frame_hash_elem;
    struct list_elem frame_list_elem;
    void* upage;    // User virtual address.
    void* kpage;    // Kernel virtual address.
    bool pinned;
    struct thread* holder_thread;
};

void init_frame_table(void);
// void finalize_frame_table();

void* alloc_page_frame(void*, bool pin);
struct frame_table_entry* evict_page(void);
void free_frame(void* kpage);
void unpin_frame(void*);
bool pin_frame(void* kpage);

unsigned frame_hash_hash_func(const struct hash_elem *e, void *aux);
bool frame_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif