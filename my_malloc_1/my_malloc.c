#include "my_malloc.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define BLOCK_SIZE sizeof(m_block)

m_block * head = NULL;

/*Create a block with information of the new allocated memory space*/
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

/*First fit strategy: find the first qualified block and return.*/
m_block * first_fit(m_block * curr, size_t size){
  while(curr->next != NULL && ((curr->next->size < size) || (curr->next->free == 0))) {
    curr = curr->next;
  }
  return curr;
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


/*malloc based on first fit strategy*/
void * ff_malloc(size_t size){
  /*current means the address of the block we will allocate */
  m_block * current;

  /*Check if header is NULL, e.g. whether the memory list is empty.
    If so, expand the current memory to get more space.*/
  if(head == NULL){
    head = create_newspace(head, size);
    if(head == NULL){
      return NULL; 
    }
    current = head;
  }

  /*Otherwise,search the memory list to see if there is a free block with enough space.
    I search the list by finding the pointer to the block before.
    So the header is a special situation.*/
  else{
    if(head->size >= size && head->free == 1){
      head->free = 0;
      current = head;
    }

    /*Search the list based on first-fit strategy.
      If not finding any qualified block, expand the memory.*/
    else{
      current = first_fit(head, size);
      if(current->next != NULL){
	current->next->free = 0;
      }
      else{
	current->next = create_newspace(current, size);
	if(current->next == NULL){
	  return NULL;
	}
      }
      current = current->next;
    }

    /*If a block's previous size is fairly bigger than the size we allocate now, 
      we split the memory region.*/
    if(current->size >= (size + BLOCK_SIZE + 32)){
      split_block(current, size);
    }
  }

  /*Return the pointer pointing at the actual data region.*/
  return  current->data_ptr;
}

/*Check if the address to be freed is a legal address we allocated before.*/
int check_ptr(void * ptr){
  /*Get the block address based on the data region pointer.*/
  m_block * m = (m_block *)ptr - 1;

  /*Check if the block address is in the memory region we have got 
    or if it is valid.*/
  if(head == NULL || m > (m_block *)sbrk(0) || m < head){
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
void ff_free(void * ptr){
  if(ptr == NULL){
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
void * bf_malloc(size_t size){
  m_block * current = NULL;

  if(head == NULL){
    head = create_newspace(head, size);
    if(head == NULL){
      return NULL; 
    }
    current = head;
  }
  /*Search the memory list by finding the pointer to the qualified block before.
    So header is a special situation.*/
  else{
    if(head->size == size && head->free == 1){
      head->free = 0;
      return head->data_ptr;
    }
    else if(head->size > size && head->free == 1){
      current = head;
    }
 
    /*Search the list with best-fit strategy.*/
    current = best_fit(head, size);
    if(current->next != NULL){
      current->next->free = 0;
    }
    else{
      current->next = create_newspace(current, size);
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

void bf_free(void * ptr){
  ff_free(ptr);
}

unsigned long get_data_segment_size(){
  unsigned long sum = 0;
  m_block * curr = head;
  while(curr != NULL){
    sum = sum + curr->size + BLOCK_SIZE;
    curr = curr->next;
  }
  return sum;
}

unsigned long get_data_segment_free_space_size(){
  unsigned long sum = 0;
  m_block * curr = head;
  while(curr != NULL){
    if(curr->free == 1){
      sum = sum + curr->size + BLOCK_SIZE;
    }
    curr = curr->next;
  }
  return sum;
}
