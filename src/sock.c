#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include "common.h"


int sock_connect(const char *remote_host, int remote_port)
{
  int                   sockfd;
  struct hostent        *he;
  struct sockaddr_in    remote_addr;

  if (!remote_host || !remote_port)
    return -1;

  if ((he = gethostbyname(remote_host)) == NULL) // XXX: gethostbyname_r? helgrind whine...
    return -2;

  remote_addr.sin_family = AF_INET;
  remote_addr.sin_addr = *((struct in_addr *)he->h_addr); // XXX: helgrind
  remote_addr.sin_port = htons(remote_port);
  memset(&(remote_addr.sin_zero), 0x00, 8);

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    return -3;

  if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof (struct sockaddr)) == -1) {
    close(sockfd);
    return 0;
  }

  return sockfd;
}



ssize_t sock_send(int sockfd, char *data, ...) //XXX: eliminate static buffer?
{
  char        buf[2048];
  va_list     args;

  buf[0] = '\0';

  va_start(args, data);
  vsnprintf(buf, sizeof buf, data, args);
  va_end(args);

  return send(sockfd, buf, strlen(buf), 0);  //XXX: did everything get sent?
}


ssize_t sock_send_nc(nntp_connection *nc, char *data, ...) //XXX: eliminate static buffer?
{
  char        buf[2048];
  va_list     args;
  ssize_t     ret;

  if (!nc->fd)
    return -1;

  buf[0] = '\0';

  va_start(args, data);
  vsnprintf(buf, sizeof buf, data, args);
  va_end(args);

  ret = send(nc->fd, buf, strlen(buf), 0);  //XXX: did everything get sent?

  //DEBUG(1, "--- SOCK_SEND_NC: [%s] %d", buf, strlen(buf));

  if (ret > 0) {
    if (nc->bits_out + ret > LONG_MAX) {
      nc->bits_out = ret - (LONG_MAX - nc->bits_out); //XXX: i'm tired. is this correct?
      nc->bits_out_rsetc++;
    }
    else
      nc->bits_out += ret; //XXX: helgrind
  }
  else {
      nc->state = NS_ERROR;
  }

  return ret;
}


void sigpipe_handler()
{
  INFO(2, "WARNING: Got signal SIGPIPE");
}
