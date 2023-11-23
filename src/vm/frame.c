#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

void init_frame_table() {
    lock_init(&frame_table_lock);
    hash_init(&frame_hash, frame_hash_hash_func, frame_hash_less_func, NULL);
    list_init(&frame_list);
}

// void finalize_frame_table() {
//     // This should be called when the system shutdown, not each process exit time.
//     // Maybe don't need this!
// }

void* alloc_page_frame(void* upage) {
    void* kpage;
    struct frame_table_entry *f;

    lock_acquire(&frame_table_lock);
    if((kpage = palloc_get_page(PAL_USER | PAL_ZERO))) {
        f = malloc(sizeof(struct frame_table_entry));
        ASSERT(f);
        f->pinned = false;
        hash_insert(&frame_hash, &f->frame_hash_elem);
        list_push_back(&frame_list, &f->frame_list_elem);
        f->kpage = kpage;
    }
    else {
        f = evict_page();
        ASSERT(f->pinned == false);
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
            //evict this!
            // swap_page(f); //TODO: to be implemented later.

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
