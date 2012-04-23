#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <assert.h>
#include "common.h"


/*void ns_disconnect(nntp_connection *nc)
{
  nntp_close(nc);
}*/


nntp_connection* ns_connect(char *server, unsigned int port)
{
  nntp_connection    *nc;
  int                ok_codes[]  = {200, 201, 0};
  int                err_codes[] = {0};

  nc = nntp_connect(server, port);

  if (nc == NULL)
    return NULL;

  /* get welcome line */
  nntp_read_response(nc, NULL, NULL, ok_codes, err_codes);

  return nc;
}


int ns_disconnect(nntp_connection *nc)
{
  int      ok_codes[]  = {205};
  int      err_codes[] = {0};
  int      ret;

  if (nc == NULL)
   return 0;

  if (nc->state == NS_CONNECTED) { 
    nntp_send_command(nc, "QUIT", NULL, NULL, ok_codes, err_codes);

    shutdown(nc->fd, SHUT_RDWR);
    ret = close(nc->fd);
  }

  free(nc);

  return ret;
}




int ns_auth(nntp_connection *nc, char *user, char *pass)
{
  int auth_ok_codes[]  = {281, 381, 0};
  int auth_err_codes[] = {502, 482, 0};
  int err;
  //char *response;
  char command[256];

  if ((strlen(user) > sizeof command - 20) || (strlen(pass) > sizeof command - 20) || !user) {
    INFO(2, "Authentication failed: malformed credentials");
    return 3;
  }

  if (user) {
    snprintf(command, sizeof command, "AUTHINFO USER %s", user);
    //err = nntp_send_command(nc, command, &response, NULL, auth_ok_codes, auth_err_codes);
    err = nntp_send_command(nc, command, NULL, NULL, auth_ok_codes, auth_err_codes);
    if (err != 0) {
      INFO(2, "Authentication failed: USER not accepted");
      return 1;
    }
  }

  if (pass) {
    snprintf(command, sizeof command, "AUTHINFO PASS %s", pass);
    //err = nntp_send_command(nc, command, &response, NULL, auth_ok_codes, auth_err_codes);
    err = nntp_send_command(nc, command, NULL, NULL, auth_ok_codes, auth_err_codes);
    if (err != 0) {
      INFO(2, "Authentication failed: PASS not accepted");
      return 2;
    }
  }

  nc->state = NS_CONNECTED;

  return 0;
}

char *mega_fast_get_data_from_server(nntp_connection *nc)
{
  char          *buf;
  char          *tmp;
  char          *ptr;
  size_t         buf_len = 4096 * 105;
  size_t         tmp_len = 0x4000;
  size_t         totbytes = 0;
  ssize_t        ret;
  struct pollfd  pfd[1];

  if (global_opts.tweak_read_buf_size)
    tmp_len = global_opts.tweak_read_buf_size + 1;

  buf = xmalloc(buf_len);
  tmp = xmalloc(tmp_len);

  buf[0] = '\0';
  tmp[0] = '\0';

  pfd[0].fd = nc->fd;
  pfd[0].events = POLLIN;

  for (;;) { /* read from socket loop */
    int pret;

    pret = poll(pfd, 1, 1000);

    if (pret > 0 && pfd[0].revents & POLLIN) {

      ret = read(nc->fd, tmp, tmp_len - 1);
      *(tmp + ret) = '\0';

      if (ret == 0 || (ret == -1 && errno != EWOULDBLOCK)) { 
        nc->state = NS_ERROR;
        break;
      }

      ptr = tmp;
      ret = strlen(ptr);
      totbytes += ret;

      if (totbytes >= buf_len) {
        buf_len += 4096 * 15;
        buf = realloc(buf, buf_len);
        assert(buf != NULL);
      }

      strcat(buf, ptr);

      /* end of article? */
      if (!strncmp((buf + totbytes - 5), "\r\n.\r\n", 5)) {
        *(buf + totbytes - 5) = '\0';
        break;
      }

    } /* POLLIN */

  } /* read from socket loop */

  free(tmp);

  if (nc->fd == -1) {
    free(buf);
    buf = NULL;
  }

  return buf;
}

