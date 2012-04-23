#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "common.h"


static pthread_mutex_t time_lock;

static yenc_info_t *
fetch_segment(pthread_t *my_id, worker_job_t *job);

void* dlworker(void *arg)
{
    pthread_t            *my_id;
    message_t            *message;
    worker_node_t        *worker_node;
    worker_job_t         *worker_job;

    my_id = arg;

    for (;;) {
        message = msg_recv_wait((int)my_id);

        switch (message->msg_id) {

            case SEGMENT_DOWNLOAD:
                worker_node = message->arg_ptr; 
                worker_job = worker_node->worker_job;

                //DEBUG(2, "worker %d downloading segment with message id %s", (int)my_id, worker_job->nzbfs->msg_id);

                if (worker_job == NULL)  {
                        msg_send((int)my_id, message->from, SEGSTAT_COMPLETED, NULL, NULL, 1);
                }

                else {
                    worker_job->yi = fetch_segment(my_id, worker_job); // XXX: helgrind

                    if (worker_job->yi == NULL) {
                        DEBUG(2, "WARNING: failed to get segment: '%s'", worker_job->nzbfs->msg_id);
                        msg_send((int)my_id, message->from, SEGSTAT_FAILED, worker_job, NULL, 1);
                    }

                    else {
                        msg_send((int)my_id, message->from, SEGSTAT_COMPLETED, worker_job, NULL, 1);
                    }
                }

                worker_node->status = WSTAT_IDLE; // XXX: helgrind

                break;

            case WORKER_TERMINATE:
                free(worker_node);
                pthread_exit(NULL); //XXX memleak, worker_job
                break;

            default:
                assert(0); // bug?
        } /* switch */ 
        msg_free(message);
    } /* for */

    return NULL;
}


///XXX: fred messed up, tom fix nntpstuff (memleaks)
static yenc_info_t *
fetch_segment(pthread_t *my_id, worker_job_t *job)
{
    size_t           bytes_downloaded = 0, data_buf_len = 4096;
    article_t        *article;
    nntp_connection  *nc;
    yenc_info_t      *yi;
    char             s_message_id[256], /*s_group[256], s_segment_num[8], */s_bytes[16];
    char             *data_buf;
    int              err;
    
    //char             *group = NULL;
    char             *message_id  = job->nzbfs->msg_id;
    size_t           segment_size = job->nzbfs->bytes;
    

    /* decoding measure */
    struct       timeval t0,t1;
    static       int init = 0;
    static struct timeval total;

    /* download speed measure */
    double dl_secs = 0;
    struct timeval dl_time;


    if (!init++) { // XXX: race
        timerclear(&total);
        pthread_mutex_init(&time_lock, NULL);
    }

    article = nntp_article_create();
    assert(article != NULL);

    snprintf(s_message_id, sizeof s_message_id, "<%s>", message_id);
    snprintf(s_bytes, sizeof s_bytes, "%d", segment_size);

    nntp_article_insert(article, "message-id:", s_message_id);
    nntp_article_insert(article, "bytes:", s_bytes);


/* *** XXX nntp_group not implemented yet
    if (global_opts.nntp_send_group_command == TRUE) {
      err = nntp_group(ns, s_group, NULL, NULL, NULL, NULL, NULL);
      if (err) {
          DEBUG(3, "WARNING: unable to send group command for article '%s', skipping!", s_message_id);
          free(ret);
          ret = NULL;
          free_connection(nc);
          goto out;
      }
    }
*/

    gettimeofday(&dl_time, NULL);

    /* get connect, start download */
    nc = get_connection(my_id);

    /* request article */
    err = nntp_get_article(nc, article);
    nntp_article_free(article);
    if (err) {
        INFO(2, "Request for article '%s' failed", s_message_id);
        free_connection(nc);
        return NULL;
    }

    /* read article data */
    data_buf = mega_fast_get_data_from_server(nc);
    if (data_buf == NULL) {
        INFO(2, "Download of article '%s' failed", s_message_id);
        free_connection(nc);
        return NULL;
    }

    data_buf_len = strlen(data_buf);
    bytes_downloaded = data_buf_len;

    gettimeofday(&t0, NULL);
    timersub(&t0, &dl_time, &dl_time);
    dl_secs += dl_time.tv_sec;
    dl_secs += dl_time.tv_usec * 0.000001;

    DEBUG(4, "+++ %u bytes in %u msecs (%u kB/sec)", 
        bytes_downloaded, (unsigned int)(dl_secs * 1000),
        (unsigned int) (bytes_downloaded / dl_secs / 1000));

    /* done with downloading, release connection-token and decode */
    free_connection(nc);

    gettimeofday(&t0, NULL);

    /* decode data */
    yi = ydecode_buffer(data_buf, data_buf_len);
    free(data_buf);

    gettimeofday(&t1, NULL);
    timersub(&t1, &t0, &t0);
    pthread_mutex_lock(&time_lock);
    timeradd(&t0, &total, &total);
    pthread_mutex_unlock(&time_lock);
    // DEBUG(2, "segment #%d decoded in %lu sec %lu ms [TOT %lu:%lu]",
    ///////    segment_num, t0.tv_sec, t0.tv_usec/1000U, total.tv_sec, total.tv_usec/1000U);

    /* XXX: check for ".." or "/" in filename */

    /* sanity check data, if we got any */
    if (yi != NULL) {

        /* sanity check key struct values -- return NULL if missing */
        if (!yi->data || !yi->size || !yi->data_size || !yi->line || !*(yi->name)) {
            INFO(2, "Sanity check of decoded data failed: key values are missing");
        }

        /* sanity check data size */
        if (!yi->has_parts && (yi->part_end - yi->part_begin) != yi->size - 1) {
            INFO(2, "Sanity check of decoded data failed: size mismatch");
        }

        /* check pcrc32 */
        if (*yi->pcrc32) {
            if(yi->pcrc32_ok != TRUE) {
                INFO(2, "Sanity check of decoded data failed: crc32 mismatch");

                /* should we fail this segment? */
                if (global_opts.fail_segment_on_checksum_mismatch)
                    ;

                job->crc_failed = TRUE;
            }
            else {
                job->crc_failed = FALSE;
            }
        }

    } 
    
    else {   /* yi is NULL, we got no data :(  */
        INFO(2, "Data decode failed for segment '%s' - no data", message_id);
        return NULL;
    }

    return yi;
}


nntp_connection* dlworker_connect(pthread_t *my_id)
{
    nntp_connection    *nc;

    DEBUG(3, "worker %d connecting to %s:%d ...", (int)my_id, global_opts.nntp_server, global_opts.nntp_port);
    if ((nc = ns_connect(global_opts.nntp_server, global_opts.nntp_port)) == NULL) {
        DEBUG(2, "connection failed!");
        puts("@@ CONNECT fail");
        return NULL;
    }

    DEBUG(3, "worker %d sending user and pw", (int)my_id); 
    if (ns_auth(nc, global_opts.nntp_user, global_opts.nntp_pass) != 0) {
        DEBUG(2, "authorization failed!");
        puts("@@ AUTH fail");
        nntp_close(nc);
        return NULL;
    }

    DEBUG(3, "worker %d authorized successfully, login complete", (int)my_id);

    return nc;
}
