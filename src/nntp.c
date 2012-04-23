#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "common.h"


void nntp_close(nntp_connection *nc)
{
  nc->state = NS_CLOSED;
  close(nc->fd);
}


nntp_connection* nntp_connect(const char *remote_host, int remote_port)
{
  nntp_connection   *nc;

  nc = xmalloc(sizeof (nntp_connection));

  nc->bits_in = 0;
  nc->bits_out = 0;
  nc->bits_in_rsetc = 0;
  nc->bits_out_rsetc = 0;

  nc->fd = sock_connect(remote_host, remote_port);

  nc->state = NS_PREAUTH;
  nc->connect_time = time(NULL);

  if (nc->fd < 1) {
    free(nc);
    nc = NULL;
  }
    
  return nc;
}
  

article_t* nntp_article_create(void)
{
  article_t *a;

  a = xmalloc(sizeof (article_t));
  memset(a, '\0', sizeof (article_t));

  return a;
}


void nntp_article_free(article_t *a)
{
  if (a->subject)    free(a->subject);
  if (a->from)       free(a->from);
  if (a->id)         free(a->id);
  if (a->newsgroups) free(a->newsgroups);
  if (a->date)       free(a->date);
  if (a->references) free(a->references);
  if (a->fu2)        free(a->fu2);
  if (a->reply_to)   free(a->reply_to);
  free(a);
}


group_t* nntp_group_create(void)
{
  group_t *g;

  g = xmalloc(sizeof (group_t));
  memset (g, '\0', sizeof (group_t));

  return g;
}

void nntp_group_free(group_t *g)
{
  if (g->name)
    free (g->name);
  free(g);
}


void nntp_debug_print_article_t(article_t *a)
{
  printf ("Subject      : %s\n", a->subject);
  printf ("From         : %s\n", a->from);
  printf ("Message-ID   : %s\n", a->id);
  printf ("Newsgroups   : %s\n", a->newsgroups);
  printf ("Date         : %s\n", a->date);
  printf ("References   : %s\n", a->references);
  printf ("Follow-up-to : %s\n", a->fu2);
  printf ("Reply-to     : %s\n", a->reply_to);
  printf ("Lines        : %i\n", a->lines);
  printf ("Bytes        : %li\n", a->bytes);
  printf ("(number      : %li)\n", a->no);
}


int nntp_article_insert(article_t *a, const char *name, const char *value)
{

  if ((name == NULL) || (a == NULL))
    return -1;

  if (strcasecmp(name, "subject:")      == 0) a->subject    =strdup(value); else
  if (strcasecmp(name, "from:")         == 0) a->from       =strdup(value); else
  if (strcasecmp(name, "date:")         == 0) a->date       =strdup(value); else
  if (strcasecmp(name, "message-id:")   == 0) a->id         =strdup(value); else
  if (strcasecmp(name, "lines:")        == 0) a->lines      =atoi(value);   else
  if (strcasecmp(name, "bytes:")        == 0) a->bytes      =atol(value);   else
  if (strcasecmp(name, "references:")   == 0) a->references =strdup(value); else
  if (strcasecmp(name, "newsgroups:")   == 0) a->newsgroups =strdup(value); else
  if (strcasecmp(name, "follow-up-to:") == 0) a->fu2        =strdup(value); else
  if (strcasecmp(name, "reply-to:")     == 0) a->reply_to   =strdup(value); else
    return -2;

  return 0;
}


int nntp_article_parse_header(article_t *a, const char *line)
{
  char   *p;
  int    res;

  if ((p = strchr(line, ' ')) == NULL)
    return -1;

  *p = '\0';
  res = nntp_article_insert(a, line, p + 1);
  *p = ' ';

  return res;
}


char* nntp_get_line(nntp_connection *nc)
{
  int       buf_len = 300;
  char      *buf;
  char      *c, *p;
  size_t    bytes = 0;
  ssize_t   ret;

  buf = xmalloc(buf_len);
  c = xmalloc(1);

  for (;;) {
    if ((ret = read(nc->fd, c, 1)) < 1) {
      if (ret == 0 || (ret == -1 && errno != EWOULDBLOCK))
          nc->state = NS_ERROR;
      free(c);
      free(buf);
      return NULL;
    }

    else if (*c == '\r')
      continue;


    else if (*c == '\n') {
      *(buf + bytes) = '\0';
      break;
    }

    else 
      *(buf + bytes++) = *c;
  }

  /* if line begins with "..", remove the first dot */
  if ((buf[0] == '.') && (buf[1] == '.')) {
    p = buf+1;
    do *p = *(p+1);
    while (*(++p));
  }

  free(c);

  return buf;
}


int nntp_read_response(nntp_connection *nc, char **response, int *rcode, int *ok_codes, int *err_codes)
{
  int    rc, i, codetype = 0;
  char   *line, line3;

  if ((nc == NULL) || (nc->fd < 1))
    return -1;

  if ((line = nntp_get_line(nc)) == NULL)
    return -1;

  if (response)
    *response = strdup(line);

  /* find return-code */
  line3 = line[3];
  line[3] = '\0';
  rc = atoi(line);
  line[3] = ' ';

  if (rcode)
    *rcode = rc;

  /* return code ok? */
  i = 0;
  while (ok_codes[i]) {
    if (ok_codes[i] == rc) {
      codetype = 1;
      break;
    }
    i++;
  }

  /* return code an error? */
  if (codetype == 0) {
    i = 0;

    while (err_codes[i]) {
      if (err_codes[i] == rc) {
        codetype = 2;
        break;
      }
      i++;
    }

  }

  /* after an 400-response the server has closed the connection */
  if (rc == 400) {

    /* XXX: close connection */

    codetype = 2;
  }

  /* after an 500-response the server didn't understand the command, but
     still responds correctly */
  if (rc >= 500)
    codetype = 2;

  /* otherwise it was in illegal response */
  if(codetype == 0)
    codetype = 2;

  if (line)
    free(line);

  return ((codetype==1) ? 0 : -1);
}


int nntp_send_command(nntp_connection *nc, char *command, char **response, int *rcode, int *ok_codes, int *err_codes)
{
  char *buf;

  if ((nc == NULL) || (nc->fd < 1))
    return -1;

  buf = xmalloc(strlen(command) + 3);
  *buf = '\0';

  strcat(buf, command);
  strcat(buf, "\r\n");

  sock_send_nc(nc, "%s", buf);
  free(buf);

  return nntp_read_response(nc, response, rcode, ok_codes, err_codes);
}


int nntp_get_article(nntp_connection *nc, article_t *ar)
{
  char    *command;
  char    *response, *p1, *p2;
  int     res;
  int     ok_codes[]  = {220, 221, 222, 223, 0};
  int     err_codes[] = {412, 420, 423, 430, 0};


  command = xmalloc(300);
  *command = '\0';

  response = NULL;

  strcat (command, "BODY");
  strcat (command, " ");
  strcat (command, ar->id);

  snprintf(command, 300 - 1, "BODY %s", ar->id);

  res = nntp_send_command(nc, command, &response, NULL, ok_codes, err_codes);

  /* if all is ok, parse the response line and get no and message-id */
  if (res == 0) {
    if (ar != NULL) {
      p1 = strchr (response, ' '); if (p1) while (*p1==' ') p1++;
      p2 = strchr (p1,' '); if (p2) *p2='\0';
      ar->no = atol(p1);

      if (ar->id == NULL) {
        p1 = p2 + 1;  while (*p1 == ' ') p1++;
        p2 = strchr(p1, ' '); if (p2) *p2 = '\0';
        ar->id = strdup(p1);
      }
    }
  }

  if (response)
    free(response);

  free(command);

  return res;
}

