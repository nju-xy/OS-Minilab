#include "common.h"

//static 
uintptr_t pm_start, pm_end, pm_used = 0;

spinlock_t alloc_lk;

//#define CORRECTNESS_FIRST
#define align(s) (((s >> 3) + 1) << 3)
//#define align_large(s) (((s >> 10) + 1) << 10)
#define info_size align(sizeof(block))
#define DIV 1024
#define IGNORE_MM 128

typedef struct Block{
    uintptr_t start;
    size_t mm_size;
    struct Block * next;
    struct Block * prev;
    int magic;// to confirm if it is a block
    int free;
}block;
block * small_head[4];
//block * large_head;
block * free_block;
uintptr_t start;



static void pmm_init() {
    pm_start = (uintptr_t)_heap.start;
    pm_end   = (uintptr_t)_heap.end;
    kmt->spin_init(&alloc_lk, "alloc lock");
#ifdef CORRECTNESS_FIRST
    start = pm_start;
#else
    for (int i = 0; i < 4; i++) {
        small_head[i] = NULL;
    }
    
    //large_head = NULL;

    free_block = (block*)pm_start;
    free_block->mm_size = pm_end - pm_start - info_size;
    free_block->free = true;
    free_block->next = NULL;
    free_block->prev = NULL;
    free_block->start = pm_start;
    free_block->magic = 777;
#endif
}

static void new_small_block(block* old, size_t size) {
    //printf("new_small_block\n");
    int sz = size;
    if(sz < DIV * 4)
        sz = DIV * 4;
    if(free_block->mm_size < sz + info_size) {
        printf("run out of memory. 0x%x left.\n", free_block->mm_size);
        assert(0);
    }

    block* new_free = (block*)(free_block->start + sz + info_size); 
    new_free->mm_size = free_block->mm_size - info_size - sz;
    new_free->start = free_block->start + sz + info_size;
    new_free->free = true;
    new_free->next = NULL;
    new_free->prev = NULL;
    new_free->magic = 777;

    free_block->mm_size = sz;
    if(old) {
        assert(old->next == NULL);
        
        free_block->prev = old;
        old->next = free_block;
    }
    else {
        assert(free_block->next == NULL);
        assert(free_block->prev == NULL);
        small_head[_cpu()] = free_block;
    }
    assert(free_block->magic == 777);
    assert(new_free->magic == 777);
    
    //printf("new_small_block 0x%x start from 0x%x\n", free_block->mm_size, free_block->start);
    free_block = new_free;
}
/*
static void new_large_block(block* old, size_t size) {
    if(free_block->mm_size < size + info_size) {
        printf("run out of memory. 0x%x left. 0x%x needed\n", free_block->mm_size, size);
        assert(0);
    }
    block* new_large = (block*)(free_block->start + free_block->mm_size - size); 
    new_large->mm_size = size;
    new_large->start = free_block->start + free_block->mm_size - size;
    new_large->free = true;
    new_large->next = NULL;
    new_large->prev = NULL;
    new_large->magic = 777;

    free_block->mm_size -= size + info_size;
    
    if(old) {
        assert(old->next == NULL);
        
        new_large->prev = old;
        old->next = new_large;
    }
    else {
        large_head = new_large;
    }
}
*/
static void divide_small(block * cur, size_t size) {
    if(cur->mm_size < size + info_size + IGNORE_MM)
        return;
    block * new_block = (block *)(cur->start + info_size + size);
    new_block->free = true;
    new_block->prev = cur;
    new_block->next = cur->next;
    new_block->start = cur->start + info_size + size;
    new_block->mm_size = cur->mm_size - info_size - size;
    new_block->magic = 777;
    if(cur->next)
        cur->next->prev = new_block;
    cur->next = new_block;
    cur->next->free = true;
    cur->mm_size = size;

    
    //printf("divide small into 0x%x and 0x%x, start 0x%x 0x%x\n", cur->mm_size, cur->next->mm_size, cur->start, cur->next->start);
    assert(cur->next->free == true);
    assert(cur->next->magic == 777);
}
/*
static void divide_large(block * cur, size_t size) {
    assert(cur->magic == 777);
    if(cur->mm_size < size + info_size + DIV * 2)
        return;
    block * new_block = cur;
    cur = (block *)(new_block->start + new_block->mm_size - size);
    cur->start = (new_block->start + new_block->mm_size - size);
    cur->mm_size = size;
    cur->magic = 777;
    
    cur->free = true;
    cur->prev = new_block->prev;
    cur->next = new_block;
    if(new_block->prev)
        new_block->prev->next = cur;
    
    new_block->prev = cur;
    new_block->mm_size = new_block->mm_size - size - info_size;

    //printf("divide large into 0x%x and 0x%x\n", cur->mm_size, cur->next->mm_size);
    assert(cur->next->free == true);
    assert(new_block->magic == 777);
}
*/
static void* find_small(size_t size) {
    block * cur = small_head[_cpu()];
    block * last = NULL;
    while(cur != NULL && !(cur->free && cur->mm_size >= size)) {
        //try to find a free suitable block
        assert(cur->magic == 777);
        if(cur->prev)
            assert(cur->prev->next = cur);
        if(cur->next)
            assert(cur->next->prev = cur);
        
        last = cur;
        cur = cur->next;
        if(cur == NULL)
            break;
    }
    if(cur != NULL) {
        // divide to save the memory
        divide_small(cur, size);
        // if found, return
        cur->free = false;
        assert(cur->magic == 777);
        return (void*)(cur->start + info_size);
    }
    new_small_block(last, size);
    return find_small(size);
}
/*
static void* find_large(size_t size) {
    block * cur = large_head;
    block * last = NULL;
    while(cur != NULL && !(cur->free && cur->mm_size >= size)) {
        //try to find a free suitable block
        assert(cur->magic == 777);
        if(cur->prev)
            assert(cur->prev->next = cur);
        if(cur->next)
            assert(cur->next->prev = cur);
        
        last = cur;
        cur = cur->next;
        if(cur != NULL) {
            if(cur->magic != 777) {
                Log("&cur->magic = %x, magic = %d, free = %d, start = 0x%x", &cur->magic, cur->magic, cur->free, cur->start);
                assert(0);
            }
        }
    }
    if(cur != NULL) {
        //if(cur->magic != 777)
        //    printf("cur is %x, magic is %d\n", cur->start, cur->magic);
        assert(cur->magic == 777);
        // divide to save the memory
        divide_large(cur, size);
        // if found, return
        cur->free = false;
        return (void*)(cur->start + info_size);
    }
    new_large_block(last, size);
    return find_large(size);
}
*/
static void *kalloc(size_t size) {
#ifdef CORRECTNESS_FIRST
    kmt->spin_lock(&alloc_lk);
    start += align(size);  // 直接分配
    void *ret = (void*)start;
    kmt->spin_unlock(&alloc_lk);
    return ret;

#else
    //fancy algorithm
    kmt->spin_lock(&alloc_lk);
    
    void * ret = NULL;
    
    //if(align(size) <= DIV)//small 
        ret = find_small(align(size));
    /*else {
        ret = find_large(align_large(size));
    }*/
    memset(ret, 0, size);
    //printf("memset 0x%x to 0x%x\n", (intptr_t)ret, (intptr_t)(ret + size));
    //printf("kalloc 0x%x, return ", size);
    //printf("0x%x\n", (uintptr_t)ret);
    pm_used += align(size);
    kmt->spin_unlock(&alloc_lk);
    return ret;

#endif
}

static void merge_small(block* b1, block* b2) {
    if(!b1 || !b2) return;
    if(b1->start + b1->mm_size + info_size != b2->start)
        return;//continuous memeory from the same CPU 
    assert(b1->next = b2);
    assert(b2->prev = b1);
    
    if(!b1->free || !b2->free) return;
    if(b1->mm_size + b2->mm_size + info_size > DIV * 4) return;
    
    //printf("merge 0x%x and 0x%x", b1->mm_size, b2->mm_size);
    
    b1->mm_size += b2->mm_size + info_size;
    
    //printf(" into 0x%x\n", b1->mm_size);
    
    b2->magic = 0;
    if(b2->next)
        b2->next->prev = b1;
    b1->next = b2->next;
    b2->magic = 0;
    b2 = NULL;
}
/*
static void merge_large(block* b1, block* b2) {
    //return;
    if(b1 == NULL || b2 == NULL) return;
    assert(b1->start + b1->mm_size + info_size == b2->start);

    assert(b1->prev = b2);
    assert(b2->next = b1);
    
    if(!b1->free || !b2->free) return;
    
    //printf("merge 0x%x and 0x%x", b1->mm_size, b2->mm_size);
    
    b1->mm_size += b2->mm_size + info_size;
    
    //printf(" into 0x%x\n", b1->mm_size);
    
    if(b2->prev)
        b2->prev->next = b1;
    else
        large_head = b1;
    b1->prev = b2->prev;
    b2->magic = 0;
    //printf("%x vanish, %x left\n", b2->start, b1->start);
    b2 = NULL;
}
*/
static void kfree(void *ptr) {
#ifdef CORRECTNESS_FIRST
    return; // memory leaks!
#else
    //complex stuffs
    kmt->spin_lock(&alloc_lk);
    //printf("free 0x%x\n", (uintptr_t)ptr);
    
    block * block_to_free = (block*)((uintptr_t)ptr - info_size);
    
    if(block_to_free == NULL) {
        kmt->spin_unlock(&alloc_lk);
        return;
    }
    if(block_to_free->magic != 777 || block_to_free->free == true) {
        //printf("free free space 0x%x\n", block_to_free->start + info_size);
        kmt->spin_unlock(&alloc_lk);
        return;
    }

    //if(block_to_free->start < free_block->start) {//small
        block_to_free->free = true;
        // merge the small free memory
        
        merge_small(block_to_free, block_to_free->next);
        merge_small(block_to_free->prev, block_to_free);
    /*}
    else {
        block_to_free->free = true;
        merge_large(block_to_free, block_to_free->prev);
        merge_large(block_to_free->next, block_to_free);
    }*/
    pm_used -= block_to_free->mm_size;
    kmt->spin_unlock(&alloc_lk);
    
#endif
}



MODULE_DEF(pmm) {
  .init = pmm_init,
  .alloc = kalloc,
  .free = kfree,
};
