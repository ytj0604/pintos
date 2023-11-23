#ifndef VM_SUPPAGE_H
#define VM_SUPPAGE_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include <hash.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

enum PAGE_STATUS_TYPE {
    FRAME_ALLOCATED, 
    LAZY_SEGMENT, 
    SWAPPED
};

// Entry of supplemental page table. 
//S-page table should be per-process, so defined in struct thread.
struct s_page_entry { 
    void *upage;
    struct hash_elem s_page_hash_entry;
    enum PAGE_STATUS_TYPE page_status_type;
    
    // If page_status_type == FRAME_ALLCATED
    struct frame_table_entry *frame_table_entry;
    // If page_status_type == LAZY_SEGMENT
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    bool writable;
    // If page_status_type == SWAPPED
    // struct swap_table_entry* swap_table_entry; TODO
};

struct s_page_entry *allocate_s_page_entry(void *upage, struct frame_table_entry *frame_table_entry, 
  struct file *file, off_t ofs, uint32_t read_bytes, bool writable);

unsigned s_page_hash_hash_func(const struct hash_elem *e, void *aux);
bool s_page_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
#endif