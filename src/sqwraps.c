#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <assert.h>
#include "queue.h"
#include "common.h"


sqlite3* sqw_db_connection(void)
{
  static sqlite3 *db = NULL;
  int ret;

  if (!db) {
    ret = sqlite3_open(sqw_db_file(), &db);
    if (ret)
      die("FATAL: (sqlite3) failed to open %s: %s\n", sqw_db_file(), sqlite3_errmsg(db));
  }

  /* xxx: remember to close connection on shutdown */ 

  return db;
}


/* simple wrapper for sqlite3_exec() */
int sqw_safe_quick_exec(sqlite3 *db, const char *sql)
{
  printf("** (sqlite3) ** QUERY: %s\n", sql);
  return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

/* same as sqw_safe_quick_exec() but trigger assert() on sqlite error */
int sqw_quick_exec(sqlite3 *db, const char *sql)
{
  int ret;

  ret = sqw_safe_quick_exec(db, sql);
  assert(ret == SQLITE_OK);

  return ret;
}


/* return pointer to db path */
char* sqw_db_file(void)
{
  static char path[PATH_MAX] = "\0";

  if (!*path)
    snprintf(path, sizeof path, "%s/.ponies/ponies.db", getenv("HOME"));

  return path;
}


/* create tables */
int sqw_init_create_tables(sqlite3 *db)
{

  /* drop all just to be sure.. */
  sqw_safe_quick_exec(db, "DROP TABLE runtime");
  sqw_safe_quick_exec(db, "DROP TABLE queue");

  /* table: runtime */

  sqw_quick_exec(db, "CREATE TABLE runtime (key varchar(16), value varchar(32))");
  sqw_quick_exec(db, "INSERT INTO runtime VALUES ('clean_shutdown', 'no')");  /* XXX: set to 'yes' on clean shutdown */

  /* table: queue */
  sqw_quick_exec(db, "CREATE TABLE queue (id integer primary key autoincrement, queue_time int, nzbpath varchar(255) unique, status int)");


  return 0;
}


/* change queue status for job */
int sqw_queue_set_status(int id, int status)
{
  char  *query;
  int   qlen;
  int   ret;

  qlen = 70;
  query = xmalloc(qlen);

  snprintf(query, qlen, "UPDATE queue SET status='%d' WHERE id='%d'", status, id);
  ret = sqw_safe_quick_exec(sqw_db_connection(), query);

  free(query);

  return ret;
}

/* add job to queue */
int sqw_queue_add(char *path, int status)
{
  char  *query;
  int   qlen, ret;
  char  **result;
  int   nrow, ncol;

  qlen = strlen(path) + 100;
  query = xmalloc(qlen);

  snprintf(query, qlen, "INSERT INTO queue VALUES (NULL, '%ld', '%s', '%d')",
      time(NULL), path, status);

  ret = sqw_safe_quick_exec(sqw_db_connection(), query);

  if (ret != SQLITE_OK) {
    free(query);
    return -1;
  }

  snprintf(query, qlen, "SELECT id FROM queue WHERE nzbpath='%s'", path);
  ret = sqlite3_get_table(sqw_db_connection(), query, &result, &nrow, &ncol, NULL);

  free(query);

  if (ret)
    die("FATAL: (sqlite3) something horrible happened :(");

  return atoi(result[1]);
}


int sqw_set_runtime_key(char *key, char *value)
{
  char *query;
  int qlen;
  int ret;

  /* first try to update the table, if row already exists */

  qlen = strlen("UPDATE runtime SET value='' WHERE key=''") + strlen(key) + strlen(value) + 1;
  query = malloc(qlen);
  snprintf(query, qlen, "UPDATE runtime SET value='%s' WHERE key='%s'", value, key);
  ret = sqw_safe_quick_exec(sqw_db_connection(), query);
  free(query);

  if (ret == SQLITE_OK) {
    /* ok, that worked, we're done */
    return 0;
  }


  /* let's insert then! */

  qlen = strlen("INSERT INTO runtime VALUES ('', '')") + strlen(key) + strlen(value) + 1;
  query = malloc(qlen);
  snprintf(query, qlen, "INSERT INTO runtime VALUES ('%s', '%s')", key, value);
  ret = sqw_safe_quick_exec(sqw_db_connection(), query);
  free(query);

  if (ret == SQLITE_OK) {
    /* ok, all good */
    return 0;
  }

  return 1;
}


char* sqw_get_runtime_key(char *key)
{
  int ret;
  char *query;
  int qlen;
  char **results;
  char *err;
  int n_rows, n_columns;
  char *retstr;

  qlen = 100;
  query = xmalloc(qlen);
 
  snprintf(query, qlen, "SELECT value FROM runtime WHERE key='%s' LIMIT 1", key);  

  ret = sqlite3_get_table(sqw_db_connection(), query, &results, &n_rows, &n_columns, &err);
  free(query);

  if (ret == SQLITE_OK) {
    retstr = malloc(strlen(results[1]) + 1);
    sprintf(retstr, "%s", results[1]);
  }
  else
    retstr = NULL;

  sqlite3_free_table(results);

  return retstr;
}


int sqw_queue_repopulate(list_t *queue_list)
{
  int ret = 0;
  char *query;
  int qlen;
  char **results;
  char *err;
  int n_rows, n_columns;
  int i;

  qlen = 100;
  query = xmalloc(qlen);
 
  snprintf(query, qlen, "SELECT nzbpath, status FROM queue ORDER BY id");  

  ret = sqlite3_get_table(sqw_db_connection(), query, &results, &n_rows, &n_columns, &err);
  free(query);
printf("--------- rows: %d cols: %d ---------\n", n_rows, n_columns);
  if (ret == SQLITE_OK && n_rows > 0) {
    for (i = 2; ; i += 2) {
      if (i > n_rows * 2)
        break;
      INFO(2, "Restoring queue state: adding %s (%d)", results[i], atoi(results[i+1]));
      queue_add_entry(queue_list, results[i], atoi(results[i+1]), QADD_IGNORE_DB);
      ret++;
    }
  }
  else
    return -1;

  sqlite3_free_table(results);
printf("------------done------------\n");
  return ret;
}
