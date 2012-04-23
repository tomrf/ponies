#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "common.h"


int master(void)
{
  list_t           *thread_list;
  thread_node_t    *thread_node;
  node_t           *node;


  INFO(2, "Spawning threads...");

  setup_socket_pool();

  thread_list = get_thread_list_ptr();

  spawn_thread(thread_list, TT_DL_MGR, download_mgr, NULL);
  spawn_thread(thread_list, TT_STATS, stats, NULL);
  spawn_thread(thread_list, TT_QUEUE_MGR, queue_mgr, NULL);
  spawn_thread(thread_list, TT_UI_MGR, ui_mgr, NULL);

  INFO(2, "Threads spawned; Ready!");

  node = thread_list->head;
  for (;;) {
    thread_node = node->data;
    pthread_join(*thread_node->tp, NULL);
    if ((node = node->next) == NULL)
       break;
  }

  die("master.c: reached end of master() !");

 
  return 0;
}


list_t* get_thread_list_ptr(void)
{
  static list_t *thread_list = NULL;

  if (thread_list == NULL)
    thread_list = list_create();

  return thread_list;
}


pthread_t* spawn_thread(list_t *thread_list, int thread_type, void* (*thread_func)(void*), pthread_t *parent)
{
  thread_node_t    *thread_node;
  pthread_t        *tp;
  char             *arg;

  list_lock(thread_list);
 
  thread_node = xmalloc(sizeof (thread_node_t));
  tp = xmalloc(sizeof (pthread_t));
  
  list_push(thread_list, thread_node);

  thread_node->thread_type = thread_type;
  thread_node->thread_function = (void *) (*thread_func);
  thread_node->tp = tp;
  thread_node->num_children = 0; 
  thread_node->child_list = NULL;
  thread_node->parent = parent;

  arg = (void*)tp;

  assert(pthread_create(tp, NULL, thread_func, arg) == 0);

  list_unlock(thread_list);

  return thread_node->tp;
}


void clean_shutdown()
{
  list_t           *thread_list;
  thread_node_t    *thread_node;
  node_t           *node;

  thread_list = get_thread_list_ptr();

  node = thread_list->head;
  for (;;) {
    thread_node = node->data;
    free(thread_node);
    if ((node = node->next) == NULL)
       break;
  }

  list_destroy(thread_list, NULL);

  exit(0);
}  

