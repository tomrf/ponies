#ifndef PROTO_H
#define PROTO_H

/* main.c */
int          main(int argc, char **argv);


/* ydecode.c */
yenc_info_t* ydecode_buffer(char *data, size_t len);
int          yenc_find_data_offset_start(char *buf, size_t buf_len);
int          yenc_find_data_offset_end(char *buf, size_t buf_len);
int          yenc_has_ybegin(char *buf, size_t len);
int          yenc_has_ypart(char *buf, size_t len);
int          yenc_find_key(char *buf, const char *key, size_t buf_len, char *value, size_t value_len);


/* conf.c */
int          conf_read(const char *path);
void         conf_set_defaults(struct config_keys *c);
void         conf_sanity_check(struct config_keys *c);


/* misc.c */
void         stolower(char *s);
void         stoupper(char *s);
void         die(char *fmt, ...);
void         debug(pid_t pid, char *file, int line, const char *function, int level, char *fmt, ...);
void         info(int level, char *fmt, ...);
char*        file_ext(char *name);
void*        xmalloc(size_t size);
int          chk_disk_full(char *path);
int          chk_running_as_root(void);
int          strend(char *s1, char *s2);


/* list.c */
list_t*      list_create(void);
node_t*      list_node_create(void *dataptr);
void         list_push(list_t *list, void *data);
void*        list_pop(list_t *list);
unsigned int list_destroy(list_t* list, int (*killer)(void *));
void         list_debug_printlist(list_t *list);
void         list_insert(list_t* list, node_t* node, node_t* newnode);
void*        list_unlink(list_t* list, node_t* node);
void*        list_to_array(list_t *list);
list_t*      list_array_to_list(void*  array, size_t len);
int          list_lock(list_t *list);
int          list_unlock(list_t *list);


/* nzb.c */
struct nzb*  nzbparse(char *path);
void         nzb_killer(struct nzb *nzb);
unsigned int nzb_segments_total(struct nzb *nzb);


/* master.c */
int          master(void);
pthread_t*   spawn_thread(list_t *tlist, int thread_type, void* (*thread_func)(void*), pthread_t *parent);
list_t*      get_thread_list_ptr(void);
void         clean_shutdown();


/* queue.c */
void*        queue_mgr(void *arg);
int          queue_scan_dir(list_t *queue_list, char *dir);
void*        queue_findfile(list_t *queue_list, char *file);
int          queue_count_status(list_t *queue_list, int status);
int          queue_add_entry(list_t *queue_list, char *path, int status, int options);


/* download.c */
void*        download_mgr(void *arg);


/* message.c */
unsigned int msg_send(int from, int to, int msg_id, void *arg_ptr, list_t *arg_list, int kekeep_pointers);
unsigned int msg_send_to_type(int from, int to_type, int msg_id, void *arg_ptr, list_t *arg_list, int keep_pointers);
message_t*   msg_recv(int recipient);
message_t*   msg_recv_wait(int recipient);
list_t*      msg_get_queue_ptr(void);
void         msg_free(message_t *message);
void         msg_debug_printmessage(message_t *message);


/* stats.c */
void*        stats(void *arg);


/* dispatch.c */
void*            dispatch(void *arg);


/* dlworker.c */
void*            dlworker(void *arg);
nntp_connection* dlworker_connect(pthread_t *my_id);


/* nntp_wrap.c */
//void               ns_disconnect(nntp_connection *ns);
nntp_connection*   ns_connect(char *server, unsigned int port);
int                ns_auth(nntp_connection *ns, char *user, char *pass);
int                ns_disconnect(nntp_connection *nc);
char*              mega_fast_get_data_from_server(nntp_connection *nc);


/* yinfo.c */
char*        yinf_get_key_value(char *s, int ignore_space);
char*        yinf_find_key_offset(char *s, char *key);
char*        yinf_find_key_offset_backwards(char *s, char *key);
int          yinf_populate_struct(yenc_info_t *yenc_info, char *data, size_t len);
int          x_atoi(const char *nptr);
long         x_atol(const char *nptr);
void         yinf_debug_print_yenc_info(yenc_info_t *yenc_info);


/* sock.c */
int          sock_connect(const char *remote_host, int remote_port);
ssize_t      sock_send(int sockfd, char *data, ...);
ssize_t      sock_send_nc(nntp_connection *nc, char *data, ...);
void         sigpipe_handler();


/* nntp.c */
article_t*   nntp_article_create(void);
void         nntp_article_free(article_t *a);
group_t*     nntp_group_create(void);
void         nntp_group_free(group_t *g);
void         nntp_debug_print_article_t(article_t *a);
int          nntp_article_insert(article_t *a, const char *name, const char *value);
int          nntp_article_parse_header(article_t *a, const char *line);
char*        nntp_get_line(nntp_connection *nc);
int          nntp_read_response(nntp_connection *nc, char **response, int *rcode, int *ok_codes, int *err_codes);
int          nntp_send_command(nntp_connection *nc, char *command, char **response, int *rcode, int *ok_codes, int *err_codes);
void         nntp_close(nntp_connection *nc);
nntp_connection*   nntp_connect(const char *remote_host, int remote_port);
int          nntp_get_article(nntp_connection *nc, article_t *ar);


/* socketpool.c */
void         free_connection(nntp_connection *conn);
nntp_connection  *get_connection(pthread_t *my_id);
int          free_all_connections(void);
list_t       *setup_socket_pool(void);


/* crc32.c */
void         chksum_crc32gentab(void);
u_int32_t    chksum_crc32(unsigned char *block, unsigned int length);

/* filepool.c */
filepool_t   * filepool_init(struct queue_node *qnode);
worker_job_t * filepool_get_job(filepool_t *pool);
int            filepool_free_segments(filepool_t *pool);
int            filepool_incomplete(filepool_t *pool);
void           filepool_segfail(filepool_t *pool, void *ptr);
void           filepool_segcomplete(filepool_t *pool, worker_job_t *job);

/* ui.c */
void*          ui_mgr(void *arg);

/* sqwraps.c */
sqlite3*       sqw_db_connection(void);
char*          sqw_db_file(void);
int            sqw_init_create_tables(sqlite3 *db);
int            sqw_quick_exec(sqlite3 *db, const char *sql);
int            sqw_safe_quick_exec(sqlite3 *db, const char *sql);
int            sqw_queue_add(char *path, int status);
int            sqw_queue_set_status(int id, int status);
char*          sqw_get_runtime_key(char *key);
int            sqw_set_runtime_key(char *key, char *value);
int            sqw_queue_repopulate(list_t *queue_list);


#endif
