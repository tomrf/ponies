#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include "common.h"


static nntp_connection *smart_connect(pthread_t *my_id);

/* global variables for socketpool */
static struct timeval   socket_idle, wait_total;
static unsigned int     socket_waiters = 0;
static pthread_cond_t   socket_cond;
static list_t           *socket_list;

/*
 * Return a connected socket, or wait.
 */
nntp_connection *get_connection(pthread_t *my_id)
{
    nntp_connection *nc;
    node_t *node;
    struct timeval now;
    int is_waiter = 0;

    list_lock(socket_list);
    for (;;) {
        node = socket_list->head;

        if (node != NULL) { 
            if (is_waiter) {
                socket_waiters--;
            } else if (timerisset(&socket_idle)) {
                gettimeofday(&now, NULL);
                timersub(&now, &socket_idle, &now);
                timeradd(&now, &wait_total, &wait_total);
                timerclear(&socket_idle);
DEBUG(2, "time socketpool was idle this time: %lu sec %lu ms [TOT %lu:%lu]",
now.tv_sec, now.tv_usec/1000U, wait_total.tv_sec, wait_total.tv_usec/1000U);

            }
            /* unlink the node (remove token) and unlock the list */
            nc = list_unlink(socket_list, node);
            list_unlock(socket_list);
            
            if (nc == NULL) /* XXX: check state instead? */
                nc = smart_connect(my_id);

            return nc;
        }

        if (!is_waiter)
            socket_waiters++;
        is_waiter = 1;
        pthread_cond_wait(&socket_cond, &socket_list->mutex);

    } /* never ending loop */
}


/*
 * Push a connection-token back in list. If we have a waiter, all is OK, 
 * else we are wasting connections! And that's bad, mkey?
 *
 * Wake up lazy sleepers if we catch any.
 */
void free_connection(nntp_connection *nc)
{
    list_lock(socket_list);
    if (socket_waiters == 0 && !timerisset(&socket_idle)) {
        /* check number of tokens, to avoid re-starting gettimeofday and
         * thus creating an incorrent measurement */
        gettimeofday(&socket_idle, NULL);
    }
    if (socket_waiters > 0)
        pthread_cond_signal(&socket_cond);
    if (nc->state != NS_CONNECTED) {
        ns_disconnect(nc);
        nc = NULL;
    }
    list_push(socket_list, nc);
    assert(socket_list->n <= global_opts.max_connections);
    /* signal is sent when we unlock this mutex */
    list_unlock(socket_list);
}

/*
 * Create a list, push empty connection-tokens, initialize pthread condition.
 * Should be called at startup, not like it is called now.
 */
list_t *setup_socket_pool(void)
{
    int i;

    socket_list = list_create();

    for (i = 0; i < global_opts.max_connections; ++i)
        list_push(socket_list, NULL);
    if (socket_list->n < 1)
        die("need at least one connection");
    pthread_cond_init(&socket_cond, NULL);

    timerclear(&socket_idle);

    return socket_list;
}

/*
 * Should be smart, but is dumb -- now a tad smarter!
 */
static nntp_connection *smart_connect(pthread_t *my_id)
{
    nntp_connection *nc;

    for (;;) {
        nc = dlworker_connect(my_id);
        if (nc == NULL) {
            INFO(2, "ITS HAMMER TIME! Connect failed, retrying in 5 seconds..");
            sleep(5);
        }
        else
            break;
     }

    return nc;
}

int free_all_connections(void)
{
    node_t *node;
    int r = 0;

    INFO(1, "Closing all NNTP connections");

    list_lock(socket_list);
    for (node = socket_list->head; node != NULL; node = node->next) {
        nntp_connection *nc = (nntp_connection*) node->data;

        if (ns_disconnect(nc))
            r++;

        node->data = NULL;
    }
    list_unlock(socket_list);

    return r;
}
