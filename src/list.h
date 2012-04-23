#ifndef LIST_H
#define LIST_H

//XXX
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind) __THROW;


typedef struct listnode_container {
  struct listnode_container    *prev;
  struct listnode_container    *next;
  void                         *data;
} node_t;


typedef struct list_state {
  unsigned long                 n;
  struct listnode_container    *head;
  struct listnode_container    *tail;
  pthread_mutex_t               mutex;
  pthread_mutexattr_t           mutexattr;
} list_t;

#endif
