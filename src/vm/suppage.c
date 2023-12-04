#include "vm/suppage.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "stdio.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"

// S-page table entry is newly allocated in following cases.
// - Stack growth. In this caes, should have allocated frame.
// - Segment load. Not load yet, but should set infos.
// - mmap. Not load yet, but should set infos.
// In the case of eviction, not allocated but already existing entry is modified.
void allocate_s_page_entry(void *upage, uint32_t kpage, 
  struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    if((unsigned)upage % PGSIZE) thread_exit();
    if(kpage != 0) { 
        struct s_page_entry *e = malloc(sizeof(struct s_page_entry));
        e->page_status_type = FRAME_ALLOCATED;
        e->upage = upage;
        e->kpage = (void*)kpage;
        e->file = NULL;
        e->ofs = 0;
        e->read_bytes = 0;
        e->zero_bytes = 0;
        e->writable = writable;
        e->modified = false;
        enum intr_level old_level = intr_disable();
        hash_insert(&thread_current()->s_page_hash, &e->s_page_hash_entry);
        intr_set_level(old_level);
    }

    else {
        int expected_count = (read_bytes + zero_bytes) / PGSIZE;
        int count = 0;
        while (read_bytes > 0 || zero_bytes > 0) {
            struct s_page_entry *e = malloc(sizeof(struct s_page_entry));
            // printf("s_page entry malloc ptr=%x\n", (uint32_t)e);
            size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
            size_t page_zero_bytes = PGSIZE - page_read_bytes;

            e->upage = upage;
            e->page_status_type = LAZY_SEGMENT;
            e->kpage = NULL;
            e->file = file;
            e->ofs = ofs;
            e->read_bytes = page_read_bytes;
            e->zero_bytes = page_zero_bytes;
            e->writable = writable;
            e->modified = false;
            
            enum intr_level old_level = intr_disable();
            hash_insert(&thread_current()->s_page_hash, &e->s_page_hash_entry);
            intr_set_level(old_level);
            read_bytes -= page_read_bytes;
            zero_bytes -= page_zero_bytes;
            upage += PGSIZE;
            ofs += PGSIZE;
            count ++;
        }
        ASSERT(expected_count == count);
    }
}

void finalize_s_page_table() {
    struct thread *t= thread_current();
    struct hash_iterator i;
    while(1) {
        hash_first (&i, &t->s_page_hash);
        if(!hash_next(&i)) break;
        struct s_page_entry *s = hash_entry(hash_cur(&i), struct s_page_entry, s_page_hash_entry);
        //Here s is any element in s-page table.
        deallocate_s_page_entry(s->upage);
    }
    ASSERT(hash_empty(&t->s_page_hash));
    hash_destroy(&t->s_page_hash, NULL);
}

enum PAGE_STATUS_TYPE check_page_status_type(void *upage) {
    upage -= (uint32_t)upage % PGSIZE;
    struct thread *t = thread_current();
    struct s_page_entry temp;
    temp.upage = upage;
    struct hash_elem *e = hash_find(&t->s_page_hash, &(temp.s_page_hash_entry));
    if(!e) return NONE;
    else {
        struct s_page_entry *ep = hash_entry(e, struct s_page_entry, s_page_hash_entry);
        return ep->page_status_type;
    }
}

void* get_kaddr(void* upage) {
    upage -= (uint32_t)upage % PGSIZE;
    struct thread *t = thread_current();
    struct s_page_entry temp;
    temp.upage = upage;
    struct hash_elem *e = hash_find(&t->s_page_hash, &(temp.s_page_hash_entry));
    if (!e) return NULL;
    struct s_page_entry *ep = hash_entry(e, struct s_page_entry, s_page_hash_entry);
    if(!(ep->page_status_type == FRAME_ALLOCATED)) return NULL;
    return ep->kpage;
}

bool check_if_writable(void *upage) {
    upage -= (uint32_t)upage % PGSIZE;
    struct thread *t = thread_current();
    struct s_page_entry temp;
    temp.upage = upage;
    struct hash_elem *e = hash_find(&t->s_page_hash, &(temp.s_page_hash_entry));
    ASSERT(e);
    struct s_page_entry *ep = hash_entry(e, struct s_page_entry, s_page_hash_entry);
    return ep->writable;
}

void handle_lazy_load(void *upage, bool write) {
    struct thread *t = thread_current();
    struct s_page_entry temp;
    temp.upage = upage;
    struct hash_elem *e = hash_find(&t->s_page_hash, &(temp.s_page_hash_entry));
    ASSERT(e);
    struct s_page_entry *ep = hash_entry(e, struct s_page_entry, s_page_hash_entry);
    ASSERT(ep->page_status_type == LAZY_SEGMENT);
    void* kpage;
    load_segment(ep->file, ep->ofs, upage, 
        ep->read_bytes, ep->zero_bytes, ep->writable, &kpage);
    ep->page_status_type = FRAME_ALLOCATED;
    ep->kpage = kpage;
    ep->modified = write;
}  

void deallocate_s_page_entry(void *upage) {
// If given upage is swapped, then delete it from swap table.
    upage -= (uint32_t)upage % PGSIZE;
    struct thread *t = thread_current();
    struct s_page_entry temp;
    temp.upage = upage;
    struct hash_elem *e = hash_find(&t->s_page_hash, &(temp.s_page_hash_entry));
    ASSERT(e);
    struct s_page_entry *ep = hash_entry(e, struct s_page_entry, s_page_hash_entry);
    hash_delete(&t->s_page_hash, e);
    pagedir_clear_page(t->pagedir, upage);
    if(ep->page_status_type == SWAPPED) delete_swap_page(ep->swap_slot_idx);
    if(ep->page_status_type == FRAME_ALLOCATED) free_frame(ep->kpage);
    free(ep);
}

unsigned s_page_hash_hash_func(const struct hash_elem *e, void *aux UNUSED) {
    struct s_page_entry *s = hash_entry(e, struct s_page_entry, s_page_hash_entry);
    return (unsigned) s->upage;
}

bool s_page_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct s_page_entry *s1 = hash_entry(a, struct s_page_entry, s_page_hash_entry);
    struct s_page_entry *s2 = hash_entry(b, struct s_page_entry, s_page_hash_entry);
    return (unsigned)s1->upage < (unsigned)s2->upage;
}