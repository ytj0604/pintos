#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include <hash.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "bitmap.h" 
#include "devices/block.h"
#include "vm/suppage.h"   

struct bitmap *swap_bitmap; // Size = swap_total_page_capacity. True means occupied.
struct block *swap_block; // Swap block device.
size_t swap_total_sector_cnt;
size_t swap_total_page_capacity; // 1 page corresponds to 8 sectors.

void init_swap_table(void);
void finalize_swap_table(void);
void swap_page(struct frame_table_entry*);
void delete_swap_page(uint32_t);
size_t sector_idx_to_slot_idx(size_t);
size_t slot_idx_to_sector_idx(size_t);
size_t get_free_swap_slot(void);
void* reload_swapped_page(void*);

#endif