#include <stdio.h>
#include <unistd.h>
#define ALIGN 8
#define CHUNK_SIZE 4096
#define O_ALLOCATED 1

#define BLK_GET_HEAD_VAL(ptr) ((size_t*)ptr)
#define BLK_GET_SIZE(ptr) ((*BLK_GET_HEAD_VAL(ptr) & ~(0x7)) - 2*sizeof(size_t))
#define BLK_GET_FOOT_PTR(ptr) (ptr + sizeof(size_t) + BLK_GET_SIZE(ptr))
#define BLK_GET_NEXT_BLK(ptr) (ptr + *BLK_GET_HEAD_VAL(ptr))
#define BLK_GET_PREV_BLK(ptr) (ptr - BLK_GET_HEAD_VAL(ptr - sizeof(size_t)))
#define BLK_IS_ALLOCATED(ptr) (*BLK_GET_HEAD_VAL(ptr) & O_ALLOCATED)
#define BLK_IS_ENOUGH(ptr, blk_siz) (BLK_GET_SIZE(ptr) > blk_siz)
#define BLK_SET_HEAD(ptr, blk_siz, flag) *BLK_GET_HEAD_VAL(ptr) = (blk_siz) | flag
#define BLK_SET_FOOT(ptr, blk_siz, flag) *BLK_GET_FOOT_PTR(ptr) = (blk_siz) | flag

typedef size_t unsigned int;

static void* heap_start;


void *first_fit(size_t blk_siz)
{
	void *blk_ptr;

	for (blk_ptr = heap_start; !BLK_IS_ALLOCATED(ptr) || BLK_GET_SIZE(ptr); blk_ptr = BLK_GET_NEXT_BLK(blk_ptr))
	{
		if (!BLK_IS_ALLOCATED(blk_ptr) && BLK_IS_ENOUGH(blk_ptr, blk_siz)) {
			size_t remain_space = BLK_GET_SIZE(blk_ptr) - blk_siz - 2*sizeof(size_t);
			assert(remain_space >= 0);
			/* If the left space is enough to form another block, then split it. */
			if (remain_space > 0) {
				BLK_SET_HEAD(blk_ptr, blk_siz + 2*sizeof(size_t), O_ALLOCATED);
				BLK_SET_FOOT(blk_ptr, blk_siz + 2*sizeof(size_t), O_ALLOCATED);
				blk_ptr = BLK_GET_NEXT_BLK(blk_ptr);
				BLK_SET_HEAD(blk_ptr, remain_space + 2*sizeof(size_t), 0);
				BLK_SET_FOOT(blk_ptr, remain_space + 2*sizeof(size_t), 0);
				blk_ptr = BLK_GET_PREV_BLK(blk_ptr);
			}
			else {
				BLK_SET_HEAD(blk_ptr, blk_siz + 4*sizeof(size_t), O_ALLOCATED);
				BLK_SET_FOOT(blk_ptr, blk_siz + 4*sizeof(size_t), O_ALLOCATED);
			}
			return blk_ptr;
		}
	}
	return NULL;
}

int heap_extend()
{
	;
}

void *jmalloc(size_t size)
{
	if (!size) {
		// or return a pointer which points to a releasable piece of memory. 
		return NULL;
	}
	size_t resize, blk_siz;
	void *res_ptr;

	resize = size % 8 ? size + 8 - (size % 8) : size;
	blk_siz = resize + 2*sizeof(size_t);
	assert(!(blk_siz % 8));

	while (!(res_ptr = first_fit(blk_siz))) {
		if (heap_extend())
			return NULL;
	}

	return res_ptr + sizeof(size_t);
}

void jfree(void *ptr)
{
	;
}

int main(int ac, char const *av[])
{
	void *ptr = sbrk(0);
	printf("heap start from %p\n", ptr);
	return 0;
}

