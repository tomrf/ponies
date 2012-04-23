#ifndef CONF_H
#define CONF_H

struct config_keys {
  int     daemon;
  int     debug;

  int     max_jobs;
  int     max_connections;

  char    nzb_queuedir_drop[PATH_MAX];
  int     nzb_queuedir_scaninterval;
  int     nzb_preparse;

  char    nntp_server[MAXHOSTNAMELEN];
  int     nntp_port;

  char    nntp_user[MAX_USERLEN];
  char    nntp_pass[MAX_PASSLEN];

  int     nntp_send_group_command;

  char    dir_download[PATH_MAX];

  char    logfile[PATH_MAX];

  int     tweak_read_buf_size;

  int     nntp_logoff_when_idle;

  int     fail_segment_on_checksum_mismatch;
  int     retry_failed_segments;

  int     free_diskspace_threshold;

  int     skip_par2;
};

#endif
