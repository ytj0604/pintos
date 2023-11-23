#include "vm/suppage.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

// S-page table entry is newly allocated in following cases.
// - Stack growth. In this caes, should have allocated frame.
// - Segment load. Not load yet, but should set infos.
// - mmap. Not load yet, but should set infos.
// In the case of eviction, not allocated but already existing entry is modified.
struct s_page_entry *allocate_s_page_entry(void *upage, struct frame_table_entry *frame_table_entry, 
  struct file *file, off_t ofs, uint32_t read_bytes, bool writable) {
    if((unsigned)upage % PGSIZE) exit(-1);

    struct s_page_entry *e = malloc(sizeof(struct s_page_entry));

    e->upage = upage;
    if(frame_table_entry) { 
        e->page_status_type = FRAME_ALLOCATED;
        e->frame_table_entry = frame_table_entry;
        e->file = NULL;
        e->ofs = 0;
        e->read_bytes = 0;
        e->writable = false;
    }
    else {
        e->page_status_type = LAZY_SEGMENT;
        e->frame_table_entry = NULL;
        e->file = file;
        e->ofs = ofs;
        e->read_bytes = read_bytes;
        e->writable = writable;
    }
    
    enum intr_level old_level = intr_disable();
    hash_insert(&thread_current()->s_page_hash, &e->s_page_hash_entry);
    intr_set_level(old_level);
    return e;
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