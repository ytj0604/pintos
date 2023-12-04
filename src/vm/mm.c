#include "mm.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "vm/suppage.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "stdio.h"
#include "vm/swap.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
// MM should persist after file close.
// After unmap, should not be able to read anymore.
// MMAP should write back only if the page was editted.
// Mapping should be process-local.
// Even after closing, deleting the original file, should be able to read through mapping.

// Had better open the file again and hold the own fd, until unmapped.

int add_file_mapping(int fd, void* uaddr) {
    // printf("addfilemapping, fd=%d\n", fd);
    struct thread *t = thread_current();
    if(t->file_descriptor[fd] == NULL) return -1;
    
    struct file* file = file_reopen(t->file_descriptor[fd]);
    // struct file* file = filesys_open("zeros"); //mmap-overlap specific test.
    int file_size = file_length(file);
    if(!file_size) return -1;

    int page_cnt = file_size % PGSIZE ? (file_size / PGSIZE + 1) : file_size/PGSIZE;
    if(!check_uaddr_safety(uaddr, page_cnt)) return -1;

    struct file_mapping_entry *file_mapping_entry = malloc(sizeof(struct file_mapping_entry));
    file_mapping_entry->mapid = fd;
    file_mapping_entry->file = file;
    file_mapping_entry->size = file_size;
    file_mapping_entry->uaddr = uaddr;

    // add suppage
    add_file_mapping_s_page_entry(uaddr, file);
    // add to file mapping hash
    hash_insert(&t->file_mapping_hash, &file_mapping_entry->file_mapping_hash_entry);
    return fd;
}

void delete_remove_file_mapping(int mapid) {
    delete_file_mapping(mapid, true);
}

void delete_file_mapping(int mapid, bool remove) {
    lock_acquire(&mm_lock);
    // Write back only the editted pages.
    struct thread *t = thread_current();
    struct file_mapping_entry temp;
    temp.mapid = mapid;
    struct hash_elem *he = hash_find(&t->file_mapping_hash, &temp.file_mapping_hash_entry);
    if(!he) {
        lock_release(&mm_lock);
        return;
    }
    struct file_mapping_entry* file_mapping_entry = 
        hash_entry(hash_find(&t->file_mapping_hash, &temp.file_mapping_hash_entry), 
        struct file_mapping_entry, file_mapping_hash_entry);
    // Cover swapped/frame allocated.

    uint32_t file_size = file_length(file_mapping_entry->file);
    uint32_t page_cnt = file_size % PGSIZE ? (file_size / PGSIZE + 1) : file_size/PGSIZE;
    uint32_t i;
    file_seek(file_mapping_entry->file, 0);
    for(i = 0; i<page_cnt; i++) {
        switch(check_page_status_type(file_mapping_entry->uaddr + i * PGSIZE)) {
            case FRAME_ALLOCATED:
            case SWAPPED:
                ;struct s_page_entry temp;
                temp.upage = file_mapping_entry->uaddr + i * PGSIZE;
                struct hash_elem *e = hash_find(&t->s_page_hash, &(temp.s_page_hash_entry));
                ASSERT(e);
                struct s_page_entry *ep = hash_entry(e, struct s_page_entry, s_page_hash_entry);
                if(ep->modified || pagedir_is_dirty(t->pagedir, file_mapping_entry->uaddr + i * PGSIZE)){
                    void *kpage = NULL;

                    while(!pin_frame(kpage)){
                        kpage = get_kaddr(file_mapping_entry->uaddr + i * PGSIZE);
                        if(!kpage) {
                            if(check_page_status_type(file_mapping_entry->uaddr + i * PGSIZE) == SWAPPED)
                            {
                                kpage = reload_swapped_page(file_mapping_entry->uaddr + i * PGSIZE);
                            }
                        }
                    }
                    // while((kpage = get_kaddr(file_mapping_entry->uaddr + i * PGSIZE)) != NULL
                    // || !pin_frame(kpage)){
                        
                    //     if(check_page_status_type(file_mapping_entry->uaddr + i * PGSIZE) == SWAPPED)
                    //     {
                    //         kpage = reload_swapped_page(file_mapping_entry->uaddr + i * PGSIZE);
                    //     }
                    // }
                    file_write(file_mapping_entry->file, file_mapping_entry->uaddr + i * PGSIZE, ep->read_bytes);
                    unpin_frame(kpage);
                    // printf("delete file mappingm write back!\n");
                }
                else {
                    // printf("delete file mapping, but not wrote back1\n");
                }
            case LAZY_SEGMENT: 
                deallocate_s_page_entry(file_mapping_entry->uaddr + i * PGSIZE);
                break;
            default:
                ASSERT(false);
        }
    }
    if(remove) {
        hash_delete(&t->file_mapping_hash, &file_mapping_entry->file_mapping_hash_entry);
    }
    file_close(file_mapping_entry->file);
    free(file_mapping_entry);
    lock_release(&mm_lock);
}

void finalize_file_mapping() {
    struct thread *t = thread_current();
    // hash_destroy(&t->file_mapping_hash, file_mapping_delete_aux_func);
    int i;
    for(i = 2; i <= t->last_fd_number; i++) {
        delete_remove_file_mapping(i);
    }
    //TODO: hash destroy.
}

void file_mapping_delete_aux_func(struct hash_elem *e, void *aux UNUSED) {
    struct file_mapping_entry *file_mapping_entry_to_delete = hash_entry(e, struct file_mapping_entry, file_mapping_hash_entry);
    delete_file_mapping(file_mapping_entry_to_delete->mapid, false);
}

bool check_uaddr_safety(void* uaddr, int page_cnt) {
    if(uaddr == NULL || (uint32_t)uaddr % PGSIZE) return false;
    int i;
    for(i = 0; i<page_cnt; i++) {
        void* upage_check = uaddr + i * PGSIZE;
        if(!is_user_vaddr(upage_check) || check_page_status_type(upage_check) != NONE) {
            return false;
        } 
    }
    return true;
}

void add_file_mapping_s_page_entry(void *upage, struct file* file) {
    uint32_t file_size = file_length(file);
    uint32_t left_file_size = file_size;
    uint32_t page_cnt = file_size % PGSIZE ? (file_size / PGSIZE + 1) : file_size/PGSIZE;
    uint32_t i;
    for(i = 0; i<page_cnt; i++) {
        uint32_t read_bytes = left_file_size > PGSIZE ? PGSIZE : left_file_size;
        uint32_t zero_bytes = PGSIZE - read_bytes;
        allocate_s_page_entry(upage + i * PGSIZE, (uint32_t)NULL, file, i * PGSIZE, read_bytes, zero_bytes, true);
        left_file_size -= read_bytes;
    }
}

unsigned file_mapping_hash_hash_func(const struct hash_elem *e, void *aux UNUSED) {
    struct file_mapping_entry *f = hash_entry(e, struct file_mapping_entry, file_mapping_hash_entry);
    return f->mapid;
}
bool file_mapping_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct file_mapping_entry *f1 = hash_entry(a, struct file_mapping_entry, file_mapping_hash_entry);
    struct file_mapping_entry *f2 = hash_entry(b, struct file_mapping_entry, file_mapping_hash_entry);
    return f1->mapid < f2->mapid;
}
