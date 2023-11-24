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

// This is used for page fault type return value.
enum PAGE_STATUS_TYPE {
    FRAME_ALLOCATED, 
    LAZY_SEGMENT, 
    SWAPPED, 
    NONE
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
    uint32_t zero_bytes;
    bool writable;
    // If page_status_type == SWAPPED
    // struct swap_table_entry* swap_table_entry; TODO
};

void allocate_s_page_entry(void *upage, struct frame_table_entry *frame_table_entry, 
  struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
enum PAGE_STATUS_TYPE check_page_fault_type(void *upage);
void handle_lazy_load(void *upage);

unsigned s_page_hash_hash_func(const struct hash_elem *e, void *aux);
bool s_page_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);
#endif