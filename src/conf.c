#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"

#define KEY(x)       ( !strcmp(cf_key, x) )
#define KEY_TRUE(x)  ( !strcmp(cf_key, x) && (!strcmp(cf_value, "yes") || !strcmp(cf_value, "true")) )
#define KEY_FALSE(x) ( !strcmp(cf_key, x) && (!strcmp(cf_value, "no") || !strcmp(cf_value, "false")) )

extern struct config_keys global_opts;

// XXX XXX XXX set config defaults?

int conf_read(const char *path)
{
  FILE            *fp;
  char            cf_key[40];
  char            cf_value[200];
  char            cf_eq;
  char            buf[250];
  unsigned long   line_num = 0;
  int             ignore_line;
  int             i;


  if ((fp = fopen(path, "r")) == NULL)
    return -1;

  for (;;) {

    memset(buf, '\0', sizeof buf);

    if (fgets(buf, sizeof buf, fp) == NULL)
      break;

    line_num++;

    ignore_line = 0;
    for (i = 0; i < strlen(buf); i++) {
      if (buf[i] == ' ' || buf[i] == '\t')
        continue;
      if (buf[i] == '#' || buf[i] == '\n')
        ignore_line = 1;
      break;
    }

    if (ignore_line == 1)
      continue;

    cf_key[0] = '\0';
    cf_value[i] = '\0';

    sscanf(buf, "%39s %c %199s", cf_key, &cf_eq, cf_value);

    stolower(cf_key);
    stolower(cf_value);

    //printf("XXX conf entry: line=%ld key=%s value=%s\n", line_num, cf_key, cf_value); // XXX

    if (!strcmp(cf_value, "true"))
      sprintf(cf_value, "yes");
    if (!strcmp(cf_value, "false"))
      sprintf(cf_value, "no");

    if KEY_TRUE("daemon")
      global_opts.daemon = TRUE;
    else if KEY_FALSE("daemon")
      global_opts.daemon = FALSE;

    else if KEY_TRUE("nntp_logoff_when_idle")
      global_opts.nntp_logoff_when_idle = TRUE;
    else if KEY_FALSE("nntp_logoff_when_idle")
      global_opts.nntp_logoff_when_idle = FALSE;

    else if KEY_TRUE("nntp_send_group_command")
      global_opts.nntp_send_group_command = TRUE;
    else if KEY_FALSE("nntp_send_group_command")
      global_opts.nntp_send_group_command = FALSE;

    else if KEY_TRUE("fail_segment_on_checksum_mismatch")
      global_opts.fail_segment_on_checksum_mismatch = TRUE;
    else if KEY_FALSE("fail_segment_on_checksum_mismatch")
      global_opts.fail_segment_on_checksum_mismatch = FALSE;

    else if KEY("debug")
      global_opts.debug = atoi(cf_value);

    else if KEY_TRUE("nzb_preparse")
      global_opts.nzb_preparse = TRUE;
    else if KEY_FALSE("nzb_preparse")
      global_opts.nzb_preparse = FALSE;

    else if KEY("max_jobs")
      global_opts.max_jobs = atoi(cf_value);

    else if KEY("max_connections")
      global_opts.max_connections = atoi(cf_value);

    else if KEY("nzb_queuedir_drop")
      snprintf(global_opts.nzb_queuedir_drop, sizeof global_opts.nzb_queuedir_drop, "%s", cf_value);

    else if KEY("nzb_queuedir_scaninterval")
      global_opts.nzb_queuedir_scaninterval = atoi(cf_value);

    else if KEY("nntp_server")
      snprintf(global_opts.nntp_server, sizeof global_opts.nntp_server, "%s", cf_value);

    else if KEY("nntp_port")
      global_opts.nntp_port = atoi(cf_value);

    else if KEY("nntp_user")
      snprintf(global_opts.nntp_user, sizeof global_opts.nntp_user, "%s", cf_value);

    else if KEY("nntp_pass")
      snprintf(global_opts.nntp_pass, sizeof global_opts.nntp_pass, "%s", cf_value);

    else if KEY("dir_download")
      snprintf(global_opts.dir_download, sizeof global_opts.dir_download, "%s", cf_value);

    else if KEY("logfile")
      snprintf(global_opts.logfile, sizeof global_opts.logfile, "%s", cf_value);

    else if KEY("tweak__read_buf_size")
      global_opts.tweak_read_buf_size = atoi(cf_value);

    else if KEY("retry_failed_segments")
      global_opts.retry_failed_segments = atoi(cf_value);

    else if KEY("free_diskspace_threshold")
      global_opts.free_diskspace_threshold  = atoi(cf_value);


    else
      die("config error: %s:%d: unknown option and/or value: `%s %c %s'", path, line_num, cf_key, cf_eq, cf_value);

  }

  fclose(fp);

  return 0;
}


void conf_set_defaults(struct config_keys *c)
{
  c->daemon = TRUE;
  c->debug = FALSE;
  *(c->logfile) = '\0';
  c->max_jobs = 1;
  c->max_connections = 5;
  *(c->dir_download) = '\0';
  *(c->nzb_queuedir_drop) = '\0';
  c->nzb_queuedir_scaninterval = 5;
  c->nzb_preparse = FALSE;
  *(c->nntp_server) = '\0';
  c->nntp_port = 119;
  *(c->nntp_user) = '\0';
  *(c->nntp_pass) = '\0';
  c->nntp_send_group_command = TRUE;
  c->tweak_read_buf_size = 0;
  c->nntp_logoff_when_idle = TRUE;
  c->fail_segment_on_checksum_mismatch = FALSE;
  c->retry_failed_segments = 0;
  c->free_diskspace_threshold = 50;
  c->skip_par2 = FALSE;
}
  


void conf_sanity_check(struct config_keys *c)
{
  struct stat st;

  /* check dir_download */
  if (stat(c->dir_download, &st) != 0)
    die("config error: dir_download (%s): %s", c->dir_download, strerror(errno));

  /* check nzb_queuedir_drop */
  if (stat(c->nzb_queuedir_drop, &st) != 0)
    die("config error: nzb_queuedir_drop (%s): %s", c->nzb_queuedir_drop, strerror(errno));

}
