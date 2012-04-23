#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "common.h"


void* ui_mgr(void *arg)
{
  message_t         *message;
  pthread_t         *my_id;

  my_id = arg;
  message = NULL;

  for (;;) {
    sleep(1);
    chk_disk_full(global_opts.dir_download);
  }

  return NULL;
}
