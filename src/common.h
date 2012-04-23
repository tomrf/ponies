#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <sqlite3.h> /* uh-oh! */

#define TRUE   1
#define FALSE  0
#define YES    TRUE
#define NO     FALSE

#define SLEEPTIME 1000*1000 / 10

#include "config.h"
#include "ydecode.h"
#include "network.h"
#include "list.h"
#include "nzb.h"
#include "threads.h"
#include "queue.h"
#include "conf.h"
#include "message.h"
#include "stats.h"
#include "dispatch.h"
#include "nntp.h"
#include "filepool.h"
#include "proto.h"

extern struct config_keys global_opts;

#ifdef USE_DEBUG
//  #define DEBUG(a,b,args...) debug(getpid(), __FILE__, __LINE__, __FUNCTION__, a, b, ## args);
  #define DEBUG(a,b,args...) debug((long int)syscall(224), __FILE__, __LINE__, __FUNCTION__, a, b, ## args);
  #define DEBUG_PREFIX "DEBUG "
#else
  #define DEBUG(a,b,args...) ;
#endif

#define INFO(a,b,args...) info(a, b, ## args);

#endif
