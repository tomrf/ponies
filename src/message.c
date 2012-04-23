#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "common.h"


unsigned int msg_send(int from, int to, int msg_id, void *arg_ptr, list_t *arg_list, int keep_pointers)
{
  list_t       *message_queue;
  message_t    *message;

  DEBUG(4, "sending message from %d to %d; msg_id=%d, arg_ptr=%p, arg_list=%p, keep_pointers=%d", from, to, msg_id, arg_ptr, arg_list, keep_pointers);

  message_queue = msg_get_queue_ptr();

  list_lock(message_queue);

  message = xmalloc(sizeof (message_t));

  DEBUG(4, "new message allocated at %p", message);

  message->from = from;
  message->to = to;
  message->msg_id = msg_id;
  message->arg_ptr = arg_ptr;
  message->arg_list = arg_list;
  message->ctime = time(NULL);
  message->atime = 0;
  message->keep_pointers = keep_pointers;

  list_push(message_queue, message);

  DEBUG(4, "message %p on list %p", message, message_queue);

  list_unlock(message_queue);

  return 1;
}


unsigned int msg_send_to_type(int from, int to_type, int msg_id, void *arg_ptr, list_t *arg_list, int keep_pointers)
{
  list_t           *thread_list;
  node_t           *node;
  thread_node_t    *thread_node;
  unsigned int     send_count = 0;

  DEBUG(4, "message from %d to TYPE %d, searching for recipients", from, to_type);

  thread_list = get_thread_list_ptr();

  list_lock(thread_list);

  if (thread_list->n == 0) {
    DEBUG(4, "thread list is empty!");
    list_unlock(thread_list);
    return 0;
  }

  node = thread_list->head;

  for (;;) {
    thread_node = node->data;

    if (thread_node->thread_type == to_type) {
      DEBUG(4, "found thread %d of type %d, calling msg_send()", thread_node->tp, to_type);
      msg_send(from, (int)thread_node->tp, msg_id, arg_ptr, arg_list, keep_pointers);
      send_count++;
    }

    if ((node = node->next) == NULL)
      break;
  }

  DEBUG(4, "done searching for recipients, did %d call(s) to msg_send()", send_count);

  list_unlock(thread_list);

  return send_count;
}


message_t* msg_recv(int recipient)
{
  list_t       *message_queue;
  node_t       *node;
  message_t    *message;
  time_t       time_now;

  message_queue = msg_get_queue_ptr();

  DEBUG(4, "looking for messages for recipient %d", recipient);

  list_lock(message_queue);

  if (message_queue->n == 0) {
    DEBUG(4, "message queue is empty!");
    list_unlock(message_queue);
    return NULL;
  }

  node = message_queue->head;

  time_now = time(NULL);

  for (;;) {
    message = node->data;

    /* check for new (atime=0) message to recipient */
    if ((int) message->to == recipient && message->atime == 0) {
      DEBUG(4, "message for recipient %d found at message %p (node %p, list %p)", recipient, message, node, message_queue);

      message->atime = time(NULL);
      list_unlock(message_queue);
      return message;
    }

    if ((node = node->next) == NULL)
      break;
  }

  DEBUG(4, "no message for recipient %d found", recipient);

  list_unlock(message_queue);

  return NULL;
}

message_t* msg_recv_wait(int recipient) 
{
    message_t *msg;

    while ((msg = msg_recv(recipient)) == NULL)
        usleep(SLEEPTIME);

    return msg;
}

void msg_free(message_t *message)
{
  list_t *message_queue;
  node_t *node;
  message_t *mnode;
  int found = 0;

  message_queue = msg_get_queue_ptr();

  list_lock(message_queue);

  DEBUG(4, "trying to free message %p", message);

  if (message_queue->n == 0) {
    DEBUG(4, "message queue is empty!");
    list_unlock(message_queue);
    return;
  }

  node = message_queue->head;

  for (;;) {

    if (node->data == message) {
      mnode = node->data;

      found = 1;

      DEBUG(4, "found message %p (node %p, list %p), freeing..", message, mnode, message_queue);

      if (mnode->keep_pointers == 0) {
        if (mnode->arg_ptr != NULL) {
          DEBUG(4, "freeing message arg_ptr %p", mnode->arg_ptr);
          free(mnode->arg_ptr);
        }
        if (mnode->arg_list != NULL)
          DEBUG(4, "freeing message arg_list %p", mnode->arg_list);
          free(mnode->arg_list);
      }

      free(node->data);
      list_unlink(message_queue, node);
      break;
    }
   
    if ((node = node->next) == NULL)
      break;

  }

  if (found == 0)
    DEBUG(4, "message %p NOT found in message queue, not freed");

  list_unlock(message_queue);
 
  return;
}


list_t* msg_get_queue_ptr(void)
{
  static list_t *message_queue = NULL;

  if (message_queue == NULL)
    message_queue = list_create();

  return message_queue;
}


void msg_debug_printmessage(message_t *message)
{
  printf("MESSAGE %p: to:           %d\n",  message, message->to);
  printf("MESSAGE %p: from:         %d\n",  message, message->from);
  printf("MESSAGE %p: ctime:        %ld\n", message, message->ctime);
  printf("MESSAGE %p: atime:        %ld\n", message, message->atime);
  printf("MESSAGE %p: msg_id:       %d\n",  message, message->msg_id);
  printf("MESSAGE %p: arg_ptr:      %p\n",  message, message->arg_ptr);
  printf("MESSAGE %p: arg_list:     %p\n",  message, message->arg_list);
  printf("MESSAGE %p: keep_pointers: %d\n",  message, message->keep_pointers);
}


