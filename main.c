#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#define ALIGN 8
#define CHUNK_SIZE 4096
#define O_ALLOCATED 1

typedef unsigned int size_j;


#define BLK_GET_HEAD_PTR(ptr) ((size_j*)(ptr))
#define BLK_GET_SIZE(ptr) ((*BLK_GET_HEAD_PTR(ptr) & ~(0x7)) - 2*sizeof(size_j))
#define BLK_GET_FOOT_PTR(ptr) ((size_j*)((ptr) + sizeof(size_j) + BLK_GET_SIZE(ptr)))
#define BLK_GET_NEXT_BLK(ptr) ((ptr) + (*BLK_GET_HEAD_PTR(ptr) & ~(0x7)))
#define BLK_GET_PREV_BLK(ptr) ((ptr) - (*BLK_GET_HEAD_PTR((ptr) - sizeof(size_j)) & ~(0x7)))
#define BLK_IS_ALLOCATED(ptr) (*BLK_GET_HEAD_PTR(ptr) & O_ALLOCATED)
#define BLK_IS_ENOUGH(ptr, blk_siz) (BLK_GET_SIZE(ptr) >= blk_siz)
#define BLK_SET_HEAD(ptr, blk_siz, flag) *BLK_GET_HEAD_PTR(ptr) = (blk_siz) | flag
#define BLK_SET_FOOT(ptr, blk_siz, flag) *BLK_GET_FOOT_PTR(ptr) = (blk_siz) | flag


void jfree(void *ptr);
void *first_fit(size_j blk_siz);
int heap_extend();
int heap_init();
void *jmalloc(size_j size);
void jcheck(void);



static void* heap_start;
static void* first_block;
static void* heap_end;
static void* last_block;

void jfree(void *ptr)
{
    void *blk_ptr;
    size_j val, next_alloc, prev_alloc;
    
    blk_ptr = ptr - sizeof(size_j);
    next_alloc = BLK_IS_ALLOCATED(BLK_GET_NEXT_BLK(blk_ptr));
    prev_alloc = BLK_IS_ALLOCATED(BLK_GET_PREV_BLK(blk_ptr));
    printf("blk_ptr=%p, next_alloc=%d, prev_alloc=%d\n", blk_ptr, next_alloc, prev_alloc);
    val = *BLK_GET_HEAD_PTR(blk_ptr) & ~0x7;

    if (prev_alloc && !next_alloc) // only next block is free.
    {
        printf("jfree: only next block is free\n");
        val += *BLK_GET_HEAD_PTR(BLK_GET_NEXT_BLK(blk_ptr));
        BLK_SET_HEAD(blk_ptr, val, 0);
        BLK_SET_FOOT(blk_ptr, val, 0);
    }

    else if (!prev_alloc && next_alloc) // only prev block is free.
    {
        printf("jfree: only prev block is free\n");
        val += *BLK_GET_HEAD_PTR(BLK_GET_PREV_BLK(blk_ptr));
        // BLK_SET_FOOT(blk_ptr, val, 0);
        // BLK_SET_HEAD(BLK_GET_HEAD_PTR(BLK_GET_PREV_BLK(blk_ptr)), val, 0);
        blk_ptr = BLK_GET_PREV_BLK(blk_ptr);
        BLK_SET_HEAD(blk_ptr, val, 0);
        BLK_SET_FOOT(blk_ptr, val, 0);

    }

    else if (prev_alloc && next_alloc) // need no merge.
    {
        printf("jfree: no need merge\n");
        BLK_SET_HEAD(blk_ptr, val, 0);
        BLK_SET_FOOT(blk_ptr, val, 0);
    }

    else
    {
        printf("jfree: both need merge\n");
        val += (*BLK_GET_HEAD_PTR(BLK_GET_PREV_BLK(blk_ptr)) +
                *BLK_GET_HEAD_PTR(BLK_GET_NEXT_BLK(blk_ptr)));
        blk_ptr = BLK_GET_HEAD_PTR(BLK_GET_PREV_BLK(blk_ptr));
        BLK_SET_HEAD(blk_ptr, val, 0);
        BLK_SET_FOOT(blk_ptr, val, 0);
    }
}

void *first_fit(size_j blk_siz)
{
    void *blk_ptr;
    int remain_space;
    for (blk_ptr = heap_start; blk_ptr != last_block; blk_ptr = BLK_GET_NEXT_BLK(blk_ptr))
    {
        if (!BLK_IS_ALLOCATED(blk_ptr) && BLK_IS_ENOUGH(blk_ptr, blk_siz)) {
            printf("blk_ptr=%p is not allocated and enough\n", blk_ptr);
            remain_space = BLK_GET_SIZE(blk_ptr) - blk_siz - 2*sizeof(size_j);
            assert(remain_space >= -8);
            /* If the left space is enough to form another block, then split it. */
            if (remain_space > 0) {
                printf("split\n");
                BLK_SET_HEAD(blk_ptr, blk_siz + 2*sizeof(size_j), O_ALLOCATED);
                BLK_SET_FOOT(blk_ptr, blk_siz + 2*sizeof(size_j), O_ALLOCATED);
                blk_ptr = BLK_GET_NEXT_BLK(blk_ptr);
                BLK_SET_HEAD(blk_ptr, remain_space + 2*sizeof(size_j), 0);
                BLK_SET_FOOT(blk_ptr, remain_space + 2*sizeof(size_j), 0);
                blk_ptr = BLK_GET_PREV_BLK(blk_ptr);
            }
            else {
                printf("can't split\n");
                BLK_SET_HEAD(blk_ptr, BLK_GET_SIZE(blk_ptr) + 2*sizeof(size_j), O_ALLOCATED);
                BLK_SET_FOOT(blk_ptr, BLK_GET_SIZE(blk_ptr) + 2*sizeof(size_j), O_ALLOCATED);
            }
            return blk_ptr;
        }
    }
    return NULL;
}

int heap_extend()
{
    void *ptr, *blk_ptr;
    if ((ptr = sbrk(CHUNK_SIZE)) == (void*)(-1))
        return -1;

    blk_ptr = last_block;
    BLK_SET_HEAD(blk_ptr, CHUNK_SIZE, 0);
    BLK_SET_FOOT(blk_ptr, CHUNK_SIZE, 0);
    last_block = BLK_GET_NEXT_BLK(blk_ptr);
    BLK_SET_HEAD(last_block, 2*sizeof(size_j), O_ALLOCATED);
    BLK_SET_FOOT(last_block, 2*sizeof(size_j), O_ALLOCATED);
    // last_block = BLK_GET_NEXT_BLK(last_block);
    // BLK_SET_HEAD(last_block, 2*sizeof(size_j), O_ALLOCATED);
    // BLK_SET_FOOT(last_block, 2*sizeof(size_j), O_ALLOCATED);


    heap_end = last_block + 2*sizeof(size_j);



    jfree(blk_ptr+sizeof(size_j));
    // jcheck();
    return 0;
}

void *jmalloc(size_j size)
{
    if (!size) {
        // or return a pointer which points to a releasable piece of memory. 
        return NULL;
    }
    size_j resize, blk_siz;
    void *res_ptr;

    resize = size % 8 ? size + 8 - (size % 8) : size;
    // blk_siz = resize + 2*sizeof(size_j);
    printf("blk_siz=%d\n", resize);
    assert(!(resize % 8));

    while (!(res_ptr = first_fit(resize))) {
        printf("first_fit fail\n");
        if (heap_extend()){
            printf("heap extend fail\n");
            return NULL;
        }
    }

    return res_ptr + sizeof(size_j);
}


int heap_init()
{
    heap_start = sbrk(5*sizeof(size_j));
    heap_start += sizeof(size_j);
    first_block = heap_start;
    BLK_SET_HEAD(heap_start, 2*sizeof(size_j), O_ALLOCATED);
    BLK_SET_FOOT(heap_start, 2*sizeof(size_j), O_ALLOCATED);
    heap_start += 2*sizeof(size_j);
    last_block = heap_start;
    BLK_SET_HEAD(last_block, 2*sizeof(size_j), O_ALLOCATED);
    BLK_SET_FOOT(last_block, 2*sizeof(size_j), O_ALLOCATED);
    heap_end = heap_start + 2*sizeof(size_j);
    jcheck();
    return 0;
}


void jcheck(void)
{
    void *blk_ptr;
    // int cnt = 5;
    printf("Start checking...\n");
    for (blk_ptr = first_block; blk_ptr != heap_end; blk_ptr = BLK_GET_NEXT_BLK(blk_ptr))
    {    
        // if (cnt-- == 0)
        //     break;
        printf("blk_ptr = %p\n", blk_ptr);
        printf("header  = %d\n", *BLK_GET_HEAD_PTR(blk_ptr));
        printf("footer  = %d\n", *BLK_GET_FOOT_PTR(blk_ptr));
        printf("------------\n");
    }
    printf("Checked finished!\n");
}

int main(int ac, char const *av[])
{
    printf("**********************************************************************\n");
    printf("**********************************************************************\n");
    printf("*****************                                    *****************\n");
    printf("***************** Welcome to Jinbao Memory Allocator *****************\n");
    printf("*****************                                    *****************\n");
    printf("**********************************************************************\n");
    printf("**********************************************************************\n");
    heap_init();
    int *i, *j, *k;
    i = jmalloc(8*sizeof(int));
    printf("heap_start at %p\n", heap_start);
    printf("heap_end   at %p\n", heap_end);
    printf("last_block at %p\n", last_block);
        printf("last_block header  = %d\n", *BLK_GET_HEAD_PTR(last_block));
        printf("last_block footer  = %d\n", *BLK_GET_FOOT_PTR(last_block));
    printf("Allocate i at %p\n", i);
    j = jmalloc(8135);
    printf("heap_start at %p\n", heap_start);
    printf("heap_end   at %p\n", heap_end);
    printf("last_block at %p\n", last_block);
        printf("last_block header  = %d\n", *BLK_GET_HEAD_PTR(last_block));
        printf("last_block footer  = %d\n", *BLK_GET_FOOT_PTR(last_block));
    printf("Allocate j at %p\n", j);
    k = jmalloc(8*sizeof(int));
    printf("heap_start at %p\n", heap_start);
    printf("heap_end   at %p\n", heap_end);
    printf("last_block at %p\n", last_block);
        printf("last_block header  = %d\n", *BLK_GET_HEAD_PTR(last_block));
        printf("last_block footer  = %d\n", *BLK_GET_FOOT_PTR(last_block));
    printf("Allocate k at %p\n", k);
    jcheck();
    jfree(i);
    jcheck();
    jfree(k);
    jcheck();
    jfree(j);
    jcheck();

    return 0;
}

