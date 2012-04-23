#ifndef THREADS_H
#define THREADS_H

#define TT_DL_MGR          1
#define TT_QUEUE_MGR       2
#define TT_UI_MGR          3
#define TT_DL_DISPATCH     4
#define TT_DL_WORKER       5
#define TT_STATS           6

typedef struct thread_node {
  int               thread_type;
  void              *(*thread_function)(void);

  pthread_t         *tp;
  pthread_t         *parent;

  unsigned int      num_children;
  list_t            *child_list;
} thread_node_t;

#endif
