#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <libgen.h>
#include "common.h"


void* download_mgr(void *arg)
{
  pthread_t            *my_id;
  list_t               *thread_list;
  message_t            *message;
  queue_node_t         *queue_node;

  my_id = arg;

  thread_list = get_thread_list_ptr();

  for (;;) {
    if ((message = msg_recv((int)my_id)) != NULL) {

      if (message->msg_id == NZB_START_DL) {
        queue_node = message->arg_ptr;

        if (queue_node->queue_status != QSTAT_WAITING) {
          DEBUG(3, "got NZB_START_DL request for queue_node with status != QSTAT_WAITING, ignoring..");
        }
        else {
          queue_node->queue_status = QSTAT_STARTING; // XXX: lock?
          queue_node->dispatch_time = time(NULL);    // XXX: lock?

          sqw_queue_set_status(queue_node->db_id, queue_node->queue_status);

          DEBUG(3, "got NZB_START_DL request for queue_node %p", queue_node);

          if (global_opts.nzb_preparse == FALSE) {
            DEBUG(3, "nzb '%s' was NOT preparsed, parsing now...", queue_node->nzb->path);
            struct nzb *old;
            old = queue_node->nzb;
            queue_node->nzb = nzbparse(queue_node->nzb->path); // XXX: lock?
            free(old);
          }

          INFO(1, "Preparing download of %s (%d kbytes in %d files)", 
                basename(queue_node->nzb->path), queue_node->nzb->size/1024, queue_node->nzb->num_files);

          queue_node->dispatch_tp = spawn_thread(thread_list, TT_DL_DISPATCH, dispatch, my_id); // XXX: lock?
          DEBUG(3, "spawned dispatcher, thread pointer %p", queue_node->dispatch_tp);

          msg_send((int)my_id, (int)queue_node->dispatch_tp, JOB_START, queue_node, NULL, 1);
          DEBUG(3, "sending JOB_START request to dispatcher %d for queue_node %p", (int)queue_node->dispatch_tp, queue_node);
        }
      
      } /* NZB_START_DL */

      else if (message->msg_id == JOB_DONE) {
        queue_node = message->arg_ptr;

        nzb_killer(queue_node->nzb); // XXX TEST XXX
        //free(queue_node->nzb);

        INFO(1, "Download of %s completed in %d seconds", basename(queue_node->nzb->path), time(NULL) - queue_node->dispatch_time);
      } /* JOB_DONE */

      msg_free(message);

    } /* new message */


    sleep(1); 
    //puts("download_mgr()");
  }

  return NULL;
}
