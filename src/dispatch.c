#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "common.h"
#include "filepool.h"



static int       d_download(struct queue_node *qnode, pthread_t *my_id);
static int       spawn_workers(list_t *worker_list, int num, pthread_t *my_id);
static void      kill_workers(list_t *worker_list, pthread_t *my_id);

static worker_node_t* 
get_free_worker(list_t *worker_list);



void* dispatch(void *arg)
{
    pthread_t             *my_id;
    message_t             *message;
    queue_node_t          *queue_node;
    int                   ret;

    my_id = arg;

    for (;;) {
        message = msg_recv_wait((int)my_id);
        switch (message->msg_id) {

            case JOB_START:
                queue_node = message->arg_ptr;
                DEBUG(3, "got JOB_START request for queue_node %p", queue_node);
                if (queue_node->queue_status == QSTAT_STARTING) {
                    DEBUG(3, "queue_node status is QSTAT_STARTING, jumping to download");
                    queue_node->queue_status = QSTAT_DOWNLOADING;

                    ret = sqw_queue_set_status(queue_node->db_id, queue_node->queue_status);
                    assert(ret == 0);

                    d_download(queue_node, my_id);

                    /* our job is now done, terminate self! */
                    queue_node->queue_status = QSTAT_FINISHED;

                    ret = sqw_queue_set_status(queue_node->db_id, queue_node->queue_status);
                    assert(ret == 0);

                    msg_send_to_type((int)my_id, TT_DL_MGR, JOB_DONE, queue_node, NULL, 1);
                    DEBUG(2, "calling pthread_exit()");
                    pthread_exit(NULL); // XXX: create a wrapper for pthread_exit() that also cleans up thread_list
                }
                break;

            default:
                    DEBUG(3, "queue_node status is NOT QSTAT_WAITING, ignoring message");
        } /* switch */
            msg_free(message);
    } /* for (;;) */
}

static int d_download(struct queue_node *qnode, pthread_t *my_id)
{
    filepool_t   *pool;
    list_t                  *worker_list;
    worker_node_t           *worker_node;
    struct nzb              *nzb;

    int start_time = time(NULL);


    /* Initialize pool */
    pool = filepool_init(qnode);
    nzb = qnode->nzb;


    /* Initialize workerstuff (XXX: do this somewhere else?) */
    worker_list = list_create();
    spawn_workers(worker_list, global_opts.max_connections, my_id);



    while (filepool_incomplete(pool) > 0) {
        
        
        if (qnode->queue_status == QSTAT_DOWNLOADING
            && filepool_free_segments(pool) > 0  
            && (worker_node = get_free_worker(worker_list)) != NULL) {

            worker_node->worker_job = filepool_get_job(pool);//XXX: helgrind
            msg_send((int)my_id, (int)worker_node->tp, 
                    SEGMENT_DOWNLOAD, worker_node, NULL, 1);

        }

        else {
            int complete, incomplete, estimate;
            message_t *msg;


            msg = msg_recv((int)my_id);
            if (msg == NULL) {
                sleep(1);
            }

            else {
                switch (msg->msg_id) {

                    case SEGSTAT_COMPLETED: /// XXX: no fucking way, SEGSTAT should not be here. we already have messageid's
                        if (msg->arg_ptr == NULL)
                            continue;

                        filepool_segcomplete(pool, msg->arg_ptr);

                        incomplete = filepool_incomplete(pool);
                        complete  = pool->segments - incomplete;
                        estimate  = (time(NULL)-start_time)/((double)complete/pool->segments)-(time(NULL)-start_time);
                        printf("\r%d/%d completed....%d min %02d sec remaining       ", 
                                complete, pool->segments, estimate/60, estimate%60);
                        fflush(stdout);
                        break;

                    case SEGSTAT_FAILED:
                        filepool_segfail(pool, msg->arg_ptr);
                        puts("SEGMENT FAILED");
                        break;

                    default:
                        printf("id: %d vs %d ptr: %p from: %d\n", msg->msg_id, SEGSTAT_COMPLETED,msg->arg_ptr, msg->from);
                        assert(0);
                }

                msg_free(msg);
            }

        }

    }

    kill_workers(worker_list, my_id);
    list_destroy(worker_list, NULL);
    
    printf("\n"); /* xxx prettify */
    INFO(2, "Dispatcher signing off.");

    return 0;
}


static int spawn_workers(list_t *worker_list, int num, pthread_t *my_id)
{
    list_t             *thread_list;
    worker_node_t      *worker_node;
    int                i;
    int                spawn_count = 0;


    thread_list = get_thread_list_ptr();

    for (i = 0; i < num; i++) {
        worker_node = xmalloc(sizeof (worker_node_t));
        /* freed when termination msg is received */

        worker_node->tp = spawn_thread(thread_list, TT_DL_WORKER, dlworker, my_id);
        worker_node->parent = my_id;
        worker_node->status = WSTAT_IDLE;

        list_push(worker_list, worker_node);
        spawn_count++;

        usleep(1);  /* avoid connect race */
    }

    return spawn_count;
}

static worker_node_t* get_free_worker(list_t *worker_list)
{
    node_t                *node;
    worker_node_t         *worker_node;

    if (worker_list->n == 0) {
        return NULL;
    }

    node = worker_list->head;

    for (;;) {

        worker_node = node->data;
        if (worker_node->status == WSTAT_IDLE) {
            worker_node->status = WSTAT_ACTIVE;
            return worker_node;
        }

        if ((node = node->next) == NULL) {
            break;
        }
    }

    return NULL;
}

static void kill_workers(list_t *worker_list, pthread_t *my_id)
{
    node_t                *node;
    worker_node_t         *worker_node;

    node = worker_list->head;
    while (node != NULL) {
        worker_node = node->data;
        msg_send((int)my_id, (int)worker_node->tp, WORKER_TERMINATE, NULL, NULL, 1);
        node = node->next;
    }
}



