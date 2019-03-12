#ifndef __MY_MALLOC__
#define __MY_MALLOC__
#include <stdlib.h>
#include <stdio.h>

/*Struct m_block is used to store the meta header information of a memory block.
  size means the data size when allocated for the first time.
  prev means the previous block of a certain block.
  next means the next block of a certain block.
  free means whether the block is freed or allocated.
  data_ptr means the pointer pointing at the actual data part.*/
typedef struct m_block_tag{
    size_t size;
    struct m_block_tag * prev;
    struct m_block_tag * next;
    int free;
    void * data_ptr;
}m_block;

void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);

void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);

#endif
