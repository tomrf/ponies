#ifndef STATS_H
#define STATS_H

struct stats_container {
  unsigned int     active_threads;
  unsigned int     active_threads_workers;

  unsigned int     active_downloads;

  unsigned int     queue_size; 
};

#endif
