#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include "common.h"


list_t* list_create(void)
{
  list_t *lptr;

  lptr = xmalloc(sizeof (list_t));

  DEBUG(5, "new list allocated at %p", lptr);

  lptr->head  = NULL;
  lptr->tail  = NULL;
  lptr->n     = 0;
  pthread_mutexattr_init(&lptr->mutexattr);
  pthread_mutexattr_settype(&lptr->mutexattr, PTHREAD_MUTEX_RECURSIVE_NP);
  pthread_mutex_init(&lptr->mutex, &lptr->mutexattr);

  return lptr;
}


node_t* list_node_create(void *dataptr)
{
  node_t *nptr;

  nptr = xmalloc(sizeof (node_t));

  DEBUG(5, "new list node allocated at %p", nptr);

  nptr->prev = NULL;
  nptr->next = NULL;
  nptr->data = dataptr;

  return nptr;
}


void list_push(list_t *list, void *data)
{
  node_t *node;

  node = list_node_create(data);
  node->data = data;

  DEBUG(5, "pushing node %p to list %p", node, list);

  list_lock(list);

  if (list->n == 0) {
    list->head = node;
    node->prev = NULL;
  }
  else {
    list->tail->next = node;
    node->prev = list->tail;
  }

  node->next = NULL;
  list->tail = node;
  list->n++;

  list_unlock(list);
}


void* list_pop(list_t *list)
{
    void* data;

    list_lock(list);

    if (list->n == 0) {
        DEBUG(5, "did not pop from list %p, list is empty!", list);
        list_unlock(list);
        return NULL;
    }

    data = list->tail->data;

    if (list->n == 1) {
        DEBUG(5, "popping %p from list %p", list->tail, list);

        free(list->tail);
        list->head = NULL;
        list->tail = NULL;
        list->n    = 0;

        list_unlock(list);
        return data;
    }

    list->tail = list->tail->prev;

    DEBUG(5, "popping %p from list %p", list->tail->next, list);

    free(list->tail->next); /* this is the old tail */
    list->tail->next = NULL;
    list->n--;

    list_unlock(list);

    return data;
}


unsigned int list_destroy(list_t* list, int (*killer)(void *))
{
    node_t* cur;
    node_t* die;
    unsigned int fail_num = 0;

    DEBUG(5, "destroying list %p", list);

    list_lock(list);

    cur = list->head;

    while (cur != NULL) {
        die = cur;
        cur = cur->next;

        DEBUG(5, "freeing node %p from list %p", die, list);

        if (killer != NULL)
            if (killer(die->data) != 0)
                fail_num++;
        free(die);
    }


    list_unlock(list);
    pthread_mutex_destroy(&list->mutex);
    pthread_mutexattr_destroy(&list->mutexattr);

    DEBUG(5, "freeing list pointer %p -- fail_num=%d", list, fail_num); 

    free(list);

    return fail_num;
}


/*
 * insert newnode in list AFTER node, if node is NULL,
 * insert newnode at list->head
 */
void list_insert(list_t* list, node_t* node, node_t* newnode)
{
    DEBUG(5, "inserting new node %p to list %p after node %p", newnode, list, node);

    list_lock(list);

    /* this is the last node, push */
    if (node->next == NULL) {
        list_unlock(list);
        return list_push(list, newnode);

    /* insert newnode at head */
    } else if (node == NULL) {
        newnode->next = list->head;
        list->head = newnode;

    /* node is neither first nor last */
    } else {
        newnode->prev = node;
        newnode->next = node->next;
        node->next = newnode;
    }

    list->n++;

    list_unlock(list);
}


void* list_unlink(list_t* list, node_t* node)
{
    void *data;
   
    DEBUG(5, "unlinking node %p from list %p", node, list);

    list_lock(list);

    /* this is the last node, pop it */
    if (node->next == NULL) {
        list_unlock(list);
        return list_pop(list);
    }
    /* this is the first node, assign head to next node */
    else if (node->prev == NULL)
        list->head = node->next;
    /* this is neither last nor first, node->prev->next is set to the next node */
    else
      node->prev->next = node->next;
    
    /* node is not the last node, this is safe */
    node->next->prev = node->prev;

    list->n--;
    data = node->data;
    free(node);

    list_unlock(list);

    return data;
}


void list_debug_printlist(list_t *list)
{
  node_t *nptr;
  unsigned long nnum = 0;

  list_lock(list);

  nptr = list->head;

  printf("list_debug_printlist: printing nodes in list @ %p\n", list);

  while (nnum++ < list->n) {
    printf("list_debug_printlist: node %ld of %ld @ %p prev=%p next=%p data=%p\n", 
          nnum, list->n, nptr, nptr->prev, nptr->next, nptr->data);
    nptr = nptr->next;
  }

  printf("list_debug_printlist: node list complete, list head=%p tail=%p\n", list->head, list->tail);

  list_unlock(list);
}  


void* list_to_array(list_t *list)
{
    node_t* cur;
    void** aptr;
    int i;

    DEBUG(5, "converting list %p to array", list);

    list_lock(list);

    aptr = xmalloc(list->n * sizeof(void *));

    cur = list->head;
    for (i = 0; i < list->n; ++i) {
        aptr[i] = cur->data;
        cur = cur->next;
    }

    list_unlock(list);

    return aptr;
}


/*
list_t* list_array_to_list(void* array, size_t len)
{
    list_t *list;
    node_t *node;
    int i;

    DEBUG(5, "coverting array %p to list", array);

    list = list_create();
    for (i = 0; i < len; ++i) {
        node = list_node_create(array[i]);
        list_push(list, node);
    }

    return list;
}
*/


int list_lock(list_t *list) // XXX: should allow to fail undetected
{
    DEBUG(6, "locked list %p", list);
    return pthread_mutex_lock(&list->mutex);
}


int list_unlock(list_t *list) // XXX: should allow to fail undetected
{
    DEBUG(6, "unlocked list %p", list);
    return pthread_mutex_unlock(&list->mutex);
}
