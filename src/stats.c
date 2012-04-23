#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "common.h"


void* stats(void *arg)
{
  message_t         *message;
  pthread_t         *my_id;
  thread_node_t     *thread_node;
  list_t            *list;
  node_t            *node;
  size_t            c1,c2,c3;
  struct stats_container *stc;

  my_id = arg;
  message = NULL;

  stc = xmalloc(sizeof (struct stats_container));
  memset(stc, '\0', sizeof (struct stats_container));

  c1 = c2 = c3 = 0;

  for (;;) {

    /* check for messages */
    message = msg_recv((int)my_id);

    /* got new message */
    if (message != NULL) {

      switch (message->msg_id) {
        case STAT_DLQ_QUEUE_SIZE:
          stc->queue_size = *(unsigned long *) message->arg_ptr;
          break;
        default:
          break;
          
      }

      msg_free(message);
    } /* GOT MESSAGE */


    /* count threads */
    c1 = c2 = 0;
    list = get_thread_list_ptr();
    list_lock(list);
    node = list->head;
    for (;;) {
      c1++;
      thread_node = node->data;
      if (thread_node->thread_type == TT_DL_WORKER)
        c2++;
      if ((node = node->next) == NULL)
         break;
    }
    list_unlock(list);
    stc->active_threads = c1 + 1;
    stc->active_threads_workers = c2;

    sleep(3);
    //printf("THREADS: %d (%d downloading)\n", stc->active_threads, stc->active_threads_workers);
  }

  return NULL;
}
