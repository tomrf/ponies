#ifndef DISPATCH_H
#define DISPATCH_H


#define WSTAT_IDLE         0
#define WSTAT_ACTIVE       1
#define WSTAT_COMPLETED    2


#define JSTAT_SUCCESS      1
#define JSTAT_FAIL         2

typedef struct worker_job {
  struct yenc_info   *yi;
  struct nzb_file    *nzbf;
  struct nzb_segment *nzbfs;
  int                status;
  int                crc_failed;
} worker_job_t;


typedef struct worker_node {
  pthread_t       *tp;
  pthread_t       *parent;
  int             status;
  worker_job_t    *worker_job;
} worker_node_t;


typedef struct dispatch_file {
  void            *map_loc;
  unsigned int    size;
  int             fd;
  unsigned int    file_num;
  unsigned int    workers;
} dispatch_file_t;


#endif
