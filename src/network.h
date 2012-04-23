#ifndef NETWORK_H
#define NETWORK_H

#define BIND(x,y)      (bind(x, (struct sockaddr *)&y,\
                            sizeof (struct sockaddr_in)) != -1)
#define REUSEADDR(x,y) (setsockopt(x, SOL_SOCKET, SO_REUSEADDR, &y,\
                            sizeof y) != -1)
#define LISTEN(x)      (listen(x, 0) != -1)
#define SETNONBLOCK(x) (fcntl(x, F_SETFL, O_NONBLOCK) != -1)
#define SOCKET(x)      ((x = socket(PF_INET, SOCK_STREAM, 0)) != -1)
#define CONNECT(x,y)   (connect(x, (struct sockaddr *)&y, \
                            sizeof(struct sockaddr)) != -1)

/* IPC-messages */

typedef enum {
    /* from clients */
    DL_START,
    DL_STOP,
    DL_SUSPEND,

    CLIENT_CONNECT,
    CLIENT_DISCONNECT,
    CLIENT_AUTH,

    /* from dispatcher <-> dlworker */
    SEGMENT_START,
    SEGMENT_STOP,
    SEGMENT_NAME,
    SEGMENT_SIZE,
    SEGMENT_ERROR,
    SEGMENT_DOWNLOAD,
    WORKER_TERMINATE,
    SEGMENT_COMPLETED,
    

    /* from download_mgr */
    JOB_START,
    JOB_DONE,

    /* from queue */
    NZB_ADDED, /* pointer to list of struct segment_node or filename? */
    NZB_REMOVED,
    NZB_START_DL,
    NZB_STATUS,
    NZB_DONE,


    /* stats stuff */
    STAT_THREADS_TOTAL,
    STAT_THREADS_RUNNING,
    STAT_DLQ_QUEUE_SIZE,

    /* network */
    PONIES_MSG,

    /* ... */

    SHUTDOWN


} msg_id_t;



/* 
 * Related to client<->ponies protocol
 */

#define MAGIC 0x6e4e7450   /* nNtP */
#define MAXFUCKOFFLEN 64
#define MAXHASHLEN 20      /* SHA1 */

struct ponies_msg {
    unsigned int magic;
    msg_id_t     type;
    size_t       len;
};

struct ponies_msg_auth {
    unsigned int version;
    char         hash[MAXHASHLEN]; /* hashed by client */
};

struct ponies_msg_welcome {
    unsigned int version;
    unsigned int id;
    /*
     * send all current stuff
     *
     * nzblist/queuelist
     * connected to nntp-server
     * number of connections
     * info about currently downloading segments
     *
     */
};

struct ponies_msg_fuckoff {
    char msg[MAXFUCKOFFLEN];
};

struct ponies_qnode {
    int          qstat;
    unsigned int post_size;
    size_t       path_len;/*size of NULL-terminated string coming.. this fall!*/
};

#endif /* NETWORK_H */
