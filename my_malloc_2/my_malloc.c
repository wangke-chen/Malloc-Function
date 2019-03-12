#include "my_malloc.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define BLOCK_SIZE sizeof(m_block)

/*lock is the mutex of synchronization primitive.*/
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/*head_lock is the header of the linked list in lock version.*/
m_block * head_lock = NULL;
/*head_nolock is the header of the thread local linked list in nolock version.*/
__thread m_block * head_nolock = NULL;

/*Create a block with information of the new allocated memory space for lock version.*/
m_block * create_newspace(m_block * curr, size_t size){
  m_block * m = sbrk(0);
  if(sbrk(BLOCK_SIZE + size) == (void *)-1){
    return NULL;
  }
  m->size = size;
  m->next = NULL;
  m->prev = curr;
  m->free = 0;
  m->data_ptr = m + 1;
  return m;
}

/*Split a fairly big block into two smaller ones.*/
void split_block(m_block * curr, size_t size){
  m_block * new = (m_block *)((char *)curr->data_ptr + size);
  new->next = curr->next;
  new->prev = curr;
  new->free = 1;
  new->size = curr->size - BLOCK_SIZE - size;
  new->data_ptr = new + 1;
  curr->next = new;
  curr->size = size;
  if(new->next != NULL){
    new->next->prev = new;
  }
}

/*Best fit strategy:find a block size is the smallest number of bytes
  greater than or equal to the required size.*/
m_block * best_fit(m_block * curr, size_t size){
  m_block * min = NULL;
  while(curr->next != NULL){
    if(curr->next->free == 0 || curr->next->size < size){
      curr = curr->next;
    }

    /*If there is a block whose size is equal to the required size, directly return.*/
    else if(curr->next->size == size){
      return curr;
    }
    else{
      if(min == NULL || min->next->size > curr->next->size){
	min = curr;
      }
      curr = curr->next;
    }
  }

  /*If no block qualified, set the min to the curr(last pointer now)*/
  if(min == NULL){
    min = curr;
  }
  return min;
}

/*Malloc based on best-fit strategy.*/
void * ts_malloc_lock(size_t size){
  /*In my implementation, I find that the whole ts_malloc_lock function
    would be a critical section. So I lock the section at the beginning.*/
  pthread_mutex_lock(&lock);
  m_block * current = NULL;

  if(head_lock == NULL){
    head_lock = create_newspace(head_lock, size);
    if(head_lock == NULL){
      pthread_mutex_unlock(&lock);
      return NULL; 
    }
    current = head_lock;
  }
  /*Search the memory list by finding the pointer to the qualified block before.
    So header is a special situation.*/
  else{
    if(head_lock->size == size && head_lock->free == 1){
      head_lock->free = 0;
      pthread_mutex_unlock(&lock);
      return head_lock->data_ptr;
    }
    else if(head_lock->size > size && head_lock->free == 1){
      current = head_lock;
    }
 
    /*Search the list with best-fit strategy.*/
    current = best_fit(head_lock, size);
    if(current->next != NULL){
      current->next->free = 0;
    }
    else{
      current->next = create_newspace(current, size);
      if(current->next == NULL){
        pthread_mutex_unlock(&lock);
	return NULL;
      }
    }
    current = current->next; 
  }

  if(current->size >= (size + BLOCK_SIZE + 32)){
    split_block(current, size);
  }

  pthread_mutex_unlock(&lock);
  return  current->data_ptr;
}

/*Check if the address to be freed is a legal address we allocated before.*/
int check_ptr(void * ptr){
  /*Get the block address based on the data region pointer.*/
  m_block * m = (m_block *)ptr - 1;

  /*Check if the block address is in the memory region we have got 
    or if it is valid.*/
  if(head_lock == NULL || m > (m_block *)sbrk(0) || m < head_lock){
    return 0;
  }
  else{
    return ptr == m->data_ptr;
  }
}

/*Merge two adjanct free block into one free block.*/
m_block * merge_free(m_block * curr){
  curr->size = curr->size + BLOCK_SIZE + curr->next->size;
  curr->next = curr->next->next;
  if(curr->next != NULL){
    curr->next->prev = curr;
  }
  return curr;
}

/*free*/
void ts_free_lock(void * ptr){
  /*In my implementation, I find that the whole ts_free_lock function
    would be a critical section. So I lock the section at the beginning.*/
  pthread_mutex_lock(&lock);
  if(ptr == NULL){
    pthread_mutex_unlock(&lock);
    return ;
  }

  /*Check if the ptr is valid.*/
  if(check_ptr(ptr)){

    /*Get the corresponding block address of this data region address.*/
    m_block * current = (m_block *)ptr - 1;
    current->free = 1;

    /*Merge the previous free region with this newly-freed region.*/
    if(current->prev != NULL && current->prev->free == 1){
      current = merge_free(current->prev);
    }

    /*Merge the next free region with this newly-freed region.*/
    if(current->next != NULL && current->next->free == 1){
      current = merge_free(current);
    }
  }
  pthread_mutex_unlock(&lock);
}

/*Create a block with information of the new allocated memory space for nolock version.*/
m_block * create_newspace_nolock(m_block * curr, size_t size){
  pthread_mutex_lock(&lock);
  m_block * m = sbrk(0);
  void * new_space = sbrk(BLOCK_SIZE + size);
  pthread_mutex_unlock(&lock);

  if(new_space == (void *)-1){
    return NULL;
  }

  m->size = size;
  m->next = NULL;
  m->prev = curr;
  m->free = 0;
  m->data_ptr = m + 1;
  return m;
}

/*Malloc based on best-fit strategy.*/
void * ts_malloc_nolock(size_t size){

  m_block * current = NULL;

  if(head_nolock == NULL){
    head_nolock = create_newspace_nolock(head_nolock, size);
    if(head_nolock == NULL){
      return NULL; 
    }
    current = head_nolock;
  }
  /*Search the memory list by finding the pointer to the qualified block before.
    So header is a special situation.*/
  else{
    if(head_nolock->size == size && head_nolock->free == 1){
      head_nolock->free = 0;
      return head_nolock->data_ptr;
    }
    else if(head_nolock->size > size && head_nolock->free == 1){
      current = head_nolock;
    }
 
    /*Search the list with best-fit strategy.*/
    current = best_fit(head_nolock, size);
    if(current->next != NULL){
      current->next->free = 0;
    }
    else{
      current->next = create_newspace_nolock(current, size);
      if(current->next == NULL){
	return NULL;
      }
    }
    current = current->next; 
  }

  if(current->size >= (size + BLOCK_SIZE + 32)){
    split_block(current, size);
  }
  return  current->data_ptr;
}

/*Check if the address to be freed is a legal address we allocated before.*/
int check_ptr_nolock(void * ptr){
  /*Get the block address based on the data region pointer.*/
  m_block * m = (m_block *)ptr - 1;

  /*Check if the block address is in the memory region we have got 
    or if it is valid.*/
  pthread_mutex_lock(&lock);
  m_block * end = sbrk(0);
  pthread_mutex_unlock(&lock);
  if(head_nolock == NULL || m > end || m < head_nolock){
    return 0;
  }
  else{
    return ptr == m->data_ptr;
  }
}


/*free*/
void ts_free_nolock(void * ptr){
  if(ptr == NULL){
    return ;
  }

  /*Check if the ptr is valid.*/
  if(check_ptr_nolock(ptr)){

    /*Get the corresponding block address of this data region address.*/
    m_block * current = (m_block *)ptr - 1;
    current->free = 1;    
  }
}


