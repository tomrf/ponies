#ifndef NNTP_H
#define NNTP_H

#define NS_CLOSED     0
#define NS_PREAUTH    1
#define NS_CONNECTED  2
#define NS_ERROR      3

typedef struct {
  int             fd;
  int             state;
  time_t          connect_time;
  long            bits_in;
  long            bits_out;
  unsigned int    bits_in_rsetc;   /* bits_in reset counter */
  unsigned int    bits_out_rsetc;  /* bits_out reset counter */
} nntp_connection;

typedef struct {
  char            *subject;
  char            *from;
  char            *id;
  char            *newsgroups;
  char            *date;
  char            *references;
  char            *fu2;
  char            *reply_to;
  int             lines;
  long            no;
  long            bytes;
} article_t;

typedef struct {
  char            *name;
  long            first;
  long            last;
  int             flags;
} group_t;


#endif
