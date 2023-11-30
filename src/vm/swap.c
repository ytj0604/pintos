#include "vm/swap.h"
#include "stdio.h"
#include "threads/thread.h"
#include "suppage.h"
#include "threads/interrupt.h"
#include "userprog/pagedir.h"

void init_swap_table() {
    swap_block = block_get_role(BLOCK_SWAP);
    swap_total_sector_cnt = block_size(swap_block);
    ASSERT(swap_total_sector_cnt % 8 == 0); //size of swap disk = multiple of page size.
    swap_total_page_capacity = swap_total_sector_cnt/8;
    swap_bitmap = bitmap_create(swap_total_page_capacity);
}

void finalize_swap_table() {
    //TODO;
    bitmap_destroy(swap_bitmap);
}

void swap_page(struct frame_table_entry *f) {
    // Evict f. F should be a frame already allocated. Also, should not be pinned.
    // Edit the owner's S-page table. 
    // Should have already acquired frame table lock.

    ASSERT(lock_held_by_current_thread(&frame_table_lock));

    ASSERT(f);
    ASSERT(!f->pinned);
    ASSERT(f->holder_thread);
    struct s_page_entry temp;
    temp.upage = f->upage;
    struct hash_elem *he = hash_find(&f->holder_thread->s_page_hash, &temp.s_page_hash_entry);
    ASSERT(he);
    struct s_page_entry *s = 
        hash_entry(he, struct s_page_entry, s_page_hash_entry);
    ASSERT(s->page_status_type == FRAME_ALLOCATED);
    ASSERT(s->kpage == f->kpage);
    
    size_t free_swap_slot_idx = get_free_swap_slot();
    size_t free_swap_sector_idx = slot_idx_to_sector_idx(free_swap_slot_idx);

    enum intr_level old_level = intr_disable();
    s->page_status_type = SWAPPED;
    s->kpage = NULL;
    s->swap_slot_idx = free_swap_slot_idx;
    pagedir_clear_page(f->holder_thread->pagedir, f->upage);
    f->upage = NULL;
    f->holder_thread = NULL;
    intr_set_level(old_level);

    int i;
    for(i = 0; i< 8 ; i++) {
        block_write(swap_block, free_swap_sector_idx + i, f->kpage + i * BLOCK_SECTOR_SIZE);
    }
}

size_t sector_idx_to_slot_idx(size_t sector_idx){
    ASSERT(sector_idx < swap_total_sector_cnt);
    return sector_idx / 8;
}

size_t slot_idx_to_sector_idx(size_t page_idx) {
    ASSERT(page_idx < swap_total_page_capacity);
    return page_idx * 8; //Beginning of the page's space.
}

size_t get_free_swap_slot() {
    // Find free page, and set bitmap.
    return bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
}

void reload_swapped_page(void * upage) {
    struct thread *t =thread_current();
    
    struct s_page_entry temp;
    temp.upage = upage;
    struct s_page_entry *s =
        hash_entry(hash_find(&t->s_page_hash, &temp.s_page_hash_entry), struct s_page_entry, s_page_hash_entry);
    ASSERT(s);
    ASSERT(s->page_status_type == SWAPPED);
    ASSERT(s->swap_slot_idx < swap_total_page_capacity);
    void * kpage = alloc_page_frame(upage, true);
    ASSERT(kpage);

    size_t swap_slot_idx = s->swap_slot_idx;
    size_t swap_sector_idx = slot_idx_to_sector_idx(swap_slot_idx);
    for(int i = 0; i< 8 ; i++) {
        block_read(swap_block, swap_sector_idx + i, kpage + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_set(swap_bitmap, swap_slot_idx, false);
    s->page_status_type = FRAME_ALLOCATED;
    s->kpage = kpage;
    pagedir_set_page(t->pagedir, upage, kpage, s->writable);

    unpin_frame(kpage);
}
    