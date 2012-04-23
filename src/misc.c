#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include "common.h"


void stolower(char *s)
{
  while (*s++)
    *s = tolower(*s);
}


void stoupper(char *s)
{
  while (*s++)
    *s = toupper(*s);
}


void die(char *fmt, ...)
{
  va_list args;

#ifdef DIE_PREFIX
  fprintf(stderr, "%s", DIE_PREFIX);
#endif

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);

  va_end(args);

  puts("");

  exit(EXIT_FAILURE);
}


void debug(pid_t pid, char *file, int line, const char *function, int level, char *fmt, ...)
{
  va_list args;

  if (global_opts.debug < level)
    return;

#ifdef DEBUG_PREFIX
  fprintf(stderr, "%s", DEBUG_PREFIX);
#endif

  fprintf(stderr, "[tid %5d] ", pid);

  va_start(args, fmt);
  fprintf(stderr, "%s:%d:%s(): [%d] ", file, line, function, level);
  vfprintf(stderr, fmt, args);

  va_end(args);

  fprintf(stderr, "\n");
}


void info(int level, char *fmt, ...)
{
  va_list args;

#ifdef INFO_PREFIX
  fprintf(stdout, "%s", INFO_PREFIX);
#endif

  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);

  fprintf(stdout, "\n");
}


char* file_ext(char *name)
{
  size_t i;
  size_t ext_offset = 0;

  i = strlen(name);

  for (; i > 0; i--) {
    if (name[i] == '.') {
      ext_offset = i;
      break;
    }
  }

  if (ext_offset == 0)
    return NULL;

  return (name + ext_offset + 1);
}

void* xmalloc(size_t size)
{
    void *r;
    r = malloc(size);
    if (r == NULL)
        die("out of memory, malloc() returned NULL");
    return r;
}


int chk_disk_full(char *path)
{
  struct statvfs   buf;
  int              ret;
  fsfilcnt_t       free_blocks, free_inodes;
  unsigned int     free_mb;
  double           free_bytes;
  
  ret = statvfs(path, &buf);
  assert(ret == 0);

  if (chk_running_as_root()) {
    free_blocks = buf.f_bfree;
    free_inodes = buf.f_ffree;
  } else {
    free_blocks = buf.f_bavail;
    free_inodes = buf.f_favail;
  }

  free_bytes = (double) (free_blocks * buf.f_frsize);
  free_mb = (free_blocks * buf.f_frsize) / (1024 * 1024);

  if (global_opts.free_diskspace_threshold != 0)
    if (global_opts.free_diskspace_threshold > free_mb)
      return 1;

  if (free_bytes == 0)
    return 1;

  return 0;
}

  
int chk_running_as_root(void)
{
  if (getuid() == 0)
    return 1;

  return 0;
}

/*
 * returns TRUE if lowercase s1 ends with lowercase s2
 */
int strend(char *s1, char *s2)
{
    int l1, l2;

    l1 = strlen(s1);
    l2 = strlen(s2);

    if (l2 > l1)
        return FALSE;
    else
        return (strcasecmp(s1+l1-l2,s2) == 0) ? TRUE : FALSE;
}
