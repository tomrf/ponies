#ifndef MESSAGE_H
#define MESSAGE_H

typedef struct message_node {
  int        from;
  int        to;

  time_t     ctime;       /* create / sent time */
  time_t     atime;       /* access / read time */

  msg_id_t   msg_id;

  void       *arg_ptr;
  list_t     *arg_list;

  int        keep_pointers;

} message_t;


#endif
