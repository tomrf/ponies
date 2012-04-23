#ifndef QUEUE_H
#define QUEUE_H


#define QSTAT_WAITING        1
#define QSTAT_STARTING       2
#define QSTAT_DOWNLOADING    3
#define QSTAT_FINISHED       4
#define QSTAT_PAUSED         5
#define QSTAT_ABORTED        6


#define QADD_IGNORE_DB       1


typedef struct queue_node {
  time_t        queue_time;
  time_t        dispatch_time;
  int           queue_status;
  pthread_t     *dispatch_tp;
  struct nzb    *nzb;
  int           db_id;
} queue_node_t;

#endif
