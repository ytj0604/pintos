#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "stdio.h"

void init_frame_table() {
    lock_init(&frame_table_lock);
    hash_init(&frame_hash, frame_hash_hash_func, frame_hash_less_func, NULL);
    list_init(&frame_list);
}

// void finalize_frame_table() {
//     // This should be called when the system shutdown, not each process exit time.
//     // Maybe don't need this!
// }

void* alloc_page_frame(void* upage, bool pin) {
    void* kpage;
    struct frame_table_entry *f;

    lock_acquire(&frame_table_lock);
    if((kpage = palloc_get_page(PAL_USER | PAL_ZERO))) {
        // size_t cur_size = hash_size(&frame_hash);
        // printf("alloc_page_frame. Before hash insert, frame_hash's current size = %u\n", cur_size);
        f = malloc(sizeof(struct frame_table_entry));
        ASSERT(f);
        f->pinned = pin;
        f->kpage = kpage;
        struct hash_elem *return_hash_elem = hash_insert(&frame_hash, &f->frame_hash_elem);
        ASSERT(!return_hash_elem); //Should be NULL.

        //For debugging.
        // printf("alloc_page_frame. After hash insert, frame_hash's current size = %u\n", hash_size(&frame_hash));
        struct frame_table_entry temp;
        temp.kpage = kpage;
        struct hash_elem *hash_elem_found = hash_find(&frame_hash, &temp.frame_hash_elem);
        ASSERT(hash_elem_found);

        list_push_back(&frame_list, &f->frame_list_elem);
    }
    else {
        f = evict_page();
        ASSERT(f->pinned == false);
        f->pinned = pin;
        kpage = f->kpage;
    }
    f->upage = upage;
    f->holder_thread = thread_current();


    lock_release(&frame_table_lock);
    return kpage;
}

struct frame_table_entry* evict_page() {
    // Second-chance algorithm.
    // In here the frame_table_lock should be already acquired.
    struct list_elem *e;
    size_t iter;
    size_t list_length = list_size(&frame_list);
    ASSERT(!list_empty(&frame_list));

    for(iter = 0; iter < 2 * list_length; iter++) { // To get free page, spin two cycle. Assume that two cycle should be enough.
        e = list_begin (&frame_list);
        struct frame_table_entry *f = list_entry (e, struct frame_table_entry, frame_list_elem);
        if(f->pinned) 
        {
            //Pop this element, and push back so that it can be considered in next cycle.
            list_remove(e);
            list_push_back(&frame_list, e);
            continue;
        }
        if(pagedir_is_accessed(f->holder_thread->pagedir, f->upage)) {
            pagedir_set_accessed(f->holder_thread->pagedir, f->upage, false);
            list_remove(e);
            list_push_back(&frame_list, e);
        }
        else {
            swap_page(f); 
            return f;            
        }
    }
    ASSERT(false);
}

void free_frame(void* kpage) {
    lock_acquire(&frame_table_lock);

    struct frame_table_entry fte_temp;
    fte_temp.kpage = kpage;
    struct hash_elem *h = hash_find(&frame_hash, &(fte_temp.frame_hash_elem));
    ASSERT(h);
    struct frame_table_entry *f = hash_entry (h, struct frame_table_entry, frame_hash_elem);
    palloc_free_page(kpage);
    hash_delete(&frame_hash, &f->frame_hash_elem);
    list_remove(&f->frame_list_elem);
    free(f);

    lock_release(&frame_table_lock);
}

bool pin_frame(void* kpage) {
    if(!kpage) return false;
    lock_acquire(&frame_table_lock);
    struct frame_table_entry fte_temp;
    fte_temp.kpage = kpage;
    struct hash_elem *h = hash_find(&frame_hash, &(fte_temp.frame_hash_elem));
    // ASSERT(h);
    if(!h) {
        lock_release(&frame_table_lock);
        return false;
    }
    struct frame_table_entry *f = hash_entry (h, struct frame_table_entry, frame_hash_elem);
    // ASSERT(!f->pinned)
    f->pinned = true;

    lock_release(&frame_table_lock);
    return true;
}

void unpin_frame(void* kpage) {
    lock_acquire(&frame_table_lock);

    struct frame_table_entry fte_temp;
    fte_temp.kpage = kpage;
    struct hash_elem *h = hash_find(&frame_hash, &(fte_temp.frame_hash_elem));
    ASSERT(h);
    struct frame_table_entry *f = hash_entry (h, struct frame_table_entry, frame_hash_elem);
    ASSERT(f->pinned)
    f->pinned = false;

    lock_release(&frame_table_lock);
}

unsigned frame_hash_hash_func(const struct hash_elem *e, void *aux UNUSED) {
    struct frame_table_entry *f = hash_entry(e, struct frame_table_entry, frame_hash_elem);
    return (unsigned)f->kpage;
}

bool frame_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    struct frame_table_entry *f1 = hash_entry(a, struct frame_table_entry, frame_hash_elem);
    struct frame_table_entry *f2 = hash_entry(b, struct frame_table_entry, frame_hash_elem);
    return (unsigned)f1->kpage < (unsigned)f2->kpage;
}
