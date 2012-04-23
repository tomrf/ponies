/*
 *
 * This file is for managing workers. In its simplest form, it returns a free
 * worker, or spawn a new one, if needed.
 *
 */

#include "common.h"

worker_node_t *get_worker()
{
    return do_get_worker(1);
}

worker_node_t *get_available_worker()
{
    return do_get_worker(0);
}

static worker_node_t *do_get_worker(int create)
{
    node_t          *node;
    list_t          *thread_list;
    thread_node_t   *tn;
    worker_node_t   *w;

    thread_list = get_thread_list_ptr();
    
    list_lock(thread_list);
    for (node = thread_list->head; node != NULL ; node = node->next) {
        tn = node->data;

        if (tn->thread_type != TT_DL_WORKER)
            continue;

        if (tn->status == WSTAT_IDLE) {
            /* found idle worker, mark as active */
            tn->status = WSTAT_ACTIVE; 
            list_unlock(thread_list);
            return tn;
        }

    } /* end traverse linked list */
    list_unlock(thread_list);

    /* found no idle workers, spawn one if create is defined */

    if (create == 1) {
        /* found no free workers, spawn one, mark as active */
        tn = spawn_thread(thread_list, TT_DL_WORKER, dlworker, NULL);
        tn->status = WSTAT_ACTIVE;
        return tn;
    }

    return NULL;
}

int set_status(pthread_t *id)
{
    list_t *thread_list;
    node_t *node;
    thread_node_t *tn;

    thread_list = get_thread_list_ptr();
    list_lock(thread_list);
    node = thread_list->head;
    while (node != NULL) {
        tn = node->data;
        ///if (

        node = node->next;
    }

    return -1;
}
