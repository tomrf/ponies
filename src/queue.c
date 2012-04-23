#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include "common.h"


void* queue_mgr(void *arg)
{
  pthread_t             *my_id;

  list_t                *queue_list;
  node_t                *node;

  queue_node_t          *queue_node;

  int                   new_entries;

  int                   jobs_downloading;
  int                   jobs_waiting;
  int                   jobs_starting;
  int                   jobs_finished;

  int                   did_close_connections = 1;

  my_id = arg;

  queue_list = list_create();

  /* load queue state from db */
  sqw_queue_repopulate(queue_list);

  for (;;) {
    new_entries = queue_scan_dir(queue_list, global_opts.nzb_queuedir_drop);

    jobs_waiting = queue_count_status(queue_list, QSTAT_WAITING);
    jobs_starting = queue_count_status(queue_list, QSTAT_STARTING);
    jobs_downloading = queue_count_status(queue_list, QSTAT_DOWNLOADING);
    jobs_finished = queue_count_status(queue_list, QSTAT_FINISHED);

    if (new_entries > 0) {
        msg_send_to_type((int)my_id, TT_UI_MGR, NZB_STATUS,
            (void *)queue_list, NULL, 1);
        DEBUG(1, "added %d new entries to queue; %d waiting, %d downloading",
            new_entries, jobs_waiting, jobs_downloading);
    }


    //XXX: for profiling
    if (jobs_waiting + jobs_starting + jobs_downloading == 0) {
        if (global_opts.nntp_logoff_when_idle == TRUE && !did_close_connections) {
            did_close_connections = 1;
            free_all_connections();
        }
        else if (!global_opts.nntp_logoff_when_idle)
            clean_shutdown();
    }
    else
      did_close_connections = 0;

    if ((jobs_downloading + jobs_starting < global_opts.max_jobs) && jobs_waiting > 0) {
      list_lock(queue_list);

      node = queue_list->head;

      for (;;) {
        queue_node = node->data;
        if (queue_node->queue_status == QSTAT_WAITING) {
          DEBUG(3, "sending NZB_START_DL request to TT_DL_MGR for queue_node %p", queue_node);
          msg_send_to_type((int)my_id, TT_DL_MGR, NZB_START_DL, queue_node, NULL, 1);
          list_unlock(queue_list);
          break;
        }

        if ((node = node->next) == NULL) {
          list_unlock(queue_list);
          break;
        }

      } /* for */

    } /* send NZB_START_DL request to download mgr*/


    //printf("queue_mgr()\n");
    sleep(3);
  }


  return NULL;
}


void* queue_findfile(list_t *queue_list, char *file)
{
  node_t               *node;
  queue_node_t         *queue_node;

  list_lock(queue_list);

  if (queue_list->n == 0) {
    list_unlock(queue_list);
    return NULL;
  }

  node = queue_list->head;

  for (;;) {
    queue_node = node->data;

    //if (queue_node->queue_status != QSTAT_FINISHED) {
      if (!strcmp(file, (const char *) queue_node->nzb->path)) {
        return queue_node;
      }
    //}

    if (node->next == NULL)
      break;

    node = node->next;
  }

  list_unlock(queue_list);

  return NULL;
}


int queue_count_status(list_t *queue_list, int status)
{
  node_t                *node;
  queue_node_t          *queue_node;
  int                   matching_jobs = 0;

  list_lock(queue_list);

  if (queue_list->n == 0) {
    list_unlock(queue_list);
    return 0;
  }

  node = queue_list->head;

  for (;;) {
    queue_node = node->data;

    if (queue_node->queue_status == status)
      matching_jobs++;

    if (node->next == NULL)
      break;

    node = node->next;
  }

  list_unlock(queue_list);

  return matching_jobs;
}


int queue_scan_dir(list_t *queue_list, char *dir)
{
  DIR                     *dp;
  struct dirent           *de;
  int                     new_entries = 0;
  char                    nzb_path[PATH_MAX];
  int                     ret;


  if ((dp = opendir(dir)) == NULL)
    return -1;

  for (;;) {

    if ((de = readdir(dp)) == NULL)
      break;

    /* is file *.nzb ? */
    if (!(strlen(de->d_name) > 3 && !strcasecmp(de->d_name + strlen(de->d_name) - 4, ".nzb")))
      continue;

    /* found file to queue, de->d_name -> nzb_path */
    snprintf(nzb_path, sizeof nzb_path, "%s/%s", dir, de->d_name);

    ret = queue_add_entry(queue_list, nzb_path, QSTAT_WAITING, 0);

    if (!ret) {
      INFO(1, "Added `%s' to queue", nzb_path);
      new_entries++;
    }
 
  } /* for */

  closedir(dp);

  return new_entries;
}


int queue_add_entry(list_t *queue_list, char *path, int status, int options)
{
  queue_node_t            *queue_node;
  FILE                    *fp;
  int                     db_id;


  /* can we open it? */
  if ((fp = fopen(path, "r")) == NULL) {
    DEBUG(1, "Failed to queue file `%s': %s", path, strerror(errno));
    INFO(2, "Failed to queue file `%s': %s", path, strerror(errno));
    return 1;
  }
  fclose(fp);

  /* is this file already queued? */
  if (queue_findfile(queue_list, path) != NULL)
    return 2;

  /* allocate a queue_node */
  if ((queue_node = xmalloc(sizeof (queue_node_t))) == NULL)
    return 4;

  /* populate queue_node */
  list_lock(queue_list);
  queue_node->queue_time = time(NULL);
  queue_node->queue_status = status;
  queue_node->dispatch_time = 0;
  queue_node->dispatch_tp = NULL;
  queue_node->db_id = -1;

  /* parse nzb-file */ 
  queue_node->nzb = nzbparse(path);
  if (queue_node->nzb == NULL) {
    /* parse failed */
    list_unlock(queue_list);
    free(queue_node);
    DEBUG(2, "Failed to parse `%s', nzbparse() returned NULL", path);
    INFO(2, "Failed to queue file `%s': nzb parse error", path);
    return 5;
  }

  /* kill parsed data if preparse is disabled */    
  if (global_opts.nzb_preparse != TRUE) {
    nzb_killer(queue_node->nzb);
    queue_node->nzb->files = NULL;
  }

  /* add to sqlite3 db, queue tabel */

  if (options != QADD_IGNORE_DB) {

    db_id = sqw_queue_add(path, queue_node->queue_status);
    if (db_id == -1) {
      /* add to sql failed, probably duplicate entry, remove this nzb from queue list */
      list_unlock(queue_list);
      free(queue_node);
      INFO(2, "Failed to queue `%s', database rejection - duplicate entry?", path);
      return 6;
    }

    queue_node->db_id = db_id;
  }

  /* push queue_node to list */
  list_push(queue_list, queue_node);
  list_unlock(queue_list);


  return 0;
}
