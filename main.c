#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#define ALIGN 8
#define CHUNK_SIZE 4096
#define O_ALLOCATED 1

#define BLK_GET_HEAD_PTR(ptr) ((size_j*)(ptr))
#define BLK_GET_SIZE(ptr) ((*BLK_GET_HEAD_PTR(ptr) & ~(0x7)) - 2*sizeof(size_j))
#define BLK_GET_FOOT_PTR(ptr) ((size_j*)((ptr) + sizeof(size_j) + BLK_GET_SIZE(ptr)))
#define BLK_GET_NEXT_BLK(ptr) ((ptr) + (*BLK_GET_HEAD_PTR(ptr) & ~(0x7)))
#define BLK_GET_PREV_BLK(ptr) ((ptr) - (*BLK_GET_HEAD_PTR((ptr) - sizeof(size_j)) & ~(0x7)))
#define BLK_IS_ALLOCATED(ptr) (*BLK_GET_HEAD_PTR(ptr) & O_ALLOCATED)
#define BLK_IS_ENOUGH(ptr, blk_siz) (BLK_GET_SIZE(ptr) >= blk_siz)
#define BLK_SET_HEAD(ptr, blk_siz, flag) *BLK_GET_HEAD_PTR(ptr) = (blk_siz) | flag
#define BLK_SET_FOOT(ptr, blk_siz, flag) *BLK_GET_FOOT_PTR(ptr) = (blk_siz) | flag

typedef unsigned int size_j;

static void* heap_start;
static void* heap_end;
static void* last_block;

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
    void *ptr;
    if ((ptr = sbrk(CHUNK_SIZE)) == (void*)(-1))
        return -1;

    BLK_SET_HEAD(last_block, CHUNK_SIZE, 0);
    BLK_SET_FOOT(last_block, CHUNK_SIZE, 0);
    last_block = BLK_GET_NEXT_BLK(last_block);
    BLK_SET_HEAD(last_block, 2*sizeof(size_j), O_ALLOCATED);
    BLK_SET_FOOT(last_block, 2*sizeof(size_j), O_ALLOCATED);
    heap_end = last_block + 2*sizeof(size_j);

    return 0;
}

int heap_init()
{
    heap_start = sbrk(3*sizeof(size_j));
    heap_start += sizeof(size_j);
    last_block = heap_start;
    BLK_SET_HEAD(last_block, 2*sizeof(size_j), O_ALLOCATED);
    BLK_SET_FOOT(last_block, 2*sizeof(size_j), O_ALLOCATED);
    heap_end = heap_start + 2*sizeof(size_j);

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
        if (heap_extend())
            return NULL;
    }

    return res_ptr + sizeof(size_j);
}

void jfree(void *ptr)
{
    void *blk_ptr;
    size_j val;
    blk_ptr = ptr - sizeof(size_j);

    if (!BLK_IS_ALLOCATED(BLK_GET_NEXT_BLK(blk_ptr))) // next block is free too.
    {
        printf("jfree: next block is free\n");
        val = *BLK_GET_HEAD_PTR(BLK_GET_NEXT_BLK(blk_ptr)) + (*BLK_GET_HEAD_PTR(blk_ptr) & ~(0x7));
        BLK_SET_HEAD(blk_ptr, val, 0);
        BLK_SET_FOOT(blk_ptr, val, 0);
    }

    else if (!BLK_IS_ALLOCATED(BLK_GET_PREV_BLK(blk_ptr))) // prev block is free too.
    {
        printf("jfree: prev block is free\n");
        val = *BLK_GET_HEAD_PTR(BLK_GET_PREV_BLK(blk_ptr)) + (*BLK_GET_HEAD_PTR(blk_ptr) & ~(0x7));
        BLK_SET_FOOT(blk_ptr, val, 0);
        BLK_SET_HEAD(BLK_GET_HEAD_PTR(BLK_GET_PREV_BLK(blk_ptr)), val, 0);
    }

    else // need no merge.
    {
        printf("jfree: no need merge\n");
        BLK_SET_HEAD(blk_ptr, (*BLK_GET_HEAD_PTR(blk_ptr) & ~(0x7)), 0);
        BLK_SET_FOOT(blk_ptr, (*BLK_GET_HEAD_PTR(blk_ptr) & ~(0x7)), 0);
    }
}

void jcheck(void)
{
    void *blk_ptr;
    // int cnt = 5;
    printf("Start checking...\n");
    for (blk_ptr = heap_start; blk_ptr != last_block; blk_ptr = BLK_GET_NEXT_BLK(blk_ptr))
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
    int *i, *j;
    i = jmalloc(8*sizeof(int));
    printf("heap_start at %p\n", heap_start);
    printf("heap_end   at %p\n", heap_end);
    printf("last_block at %p\n", last_block);
    printf("Allocate i at %p\n", i);
    jcheck();
    jfree(i);
    jcheck();
    j = jmalloc(8*sizeof(long));
    printf("heap_start at %p\n", heap_start);
    printf("heap_end   at %p\n", heap_end);
    printf("last_block at %p\n", last_block);
    printf("Allocate i at %p\n", j);
    jcheck();

    return 0;
}

