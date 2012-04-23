#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sqlite3.h>
#include "common.h"


struct config_keys global_opts;


int main(int argc, char **argv)
{
  char     config_file[PATH_MAX];
  char     poniespath[PATH_MAX];
  DIR      *dp;
  sqlite3  *db;
  int      ret;

  /* read config */

  sprintf(config_file, "../testing/testing_config.txt");
  INFO(1, "Parsing configuration from `%s'", config_file);
  conf_set_defaults(&global_opts);

  if (conf_read(config_file) == -1)
    die("error: unable to open config file `%s`: %s", config_file, strerror(errno));
  conf_sanity_check(&global_opts);

  if (argv[1])
    global_opts.debug = atoi(argv[1]);


  /* check for or create ~/.ponies/ */

  if (!getenv("HOME"))
    die("FATAL: $HOME not set");

  snprintf(poniespath, sizeof poniespath, "%s/.ponies/", getenv("HOME"));

  if (((dp = opendir(poniespath))) == NULL) {
    if (mkdir(poniespath, S_IRWXU) == -1) {
      die("FATAL: failed to create directory %s: %s\n", poniespath, strerror(errno));
    }
    INFO(1, "Created directory %s", poniespath);
  }
  else
    closedir(dp);

  
  /* check for or create ~/.ponies/state.db */
  
  ret = sqlite3_open(sqw_db_file(), &db);
  if (ret)
    die("FATAL: (sqlite3) failed to open %s: %s\n", sqw_db_file(), sqlite3_errmsg(db));


  /* is this a newly created database? */

  ret = sqw_safe_quick_exec(db, "SELECT * FROM runtime LIMIT 1");
  if (ret) {
    /* ..yes it is, let's create some tables.. */
    INFO(1, "Creating db tables..");
    sqw_init_create_tables(db);
  }


  /* close db connection */

  sqlite3_close(db);


  /* set up handlers */

  signal(SIGPIPE, sigpipe_handler);


  /* other stuff */

  chksum_crc32gentab();


  /* call thread master */

  INFO(1, "Handing control over to thread master");
  master();


  return EXIT_SUCCESS;
}
