/* 
 * mapping between nzb-filenum, filename and fd / mmaped location
 */

#ifndef FILEPOOL
#define FILEPOOL

typedef struct {
    struct queue_node *qnode;
    list_t         *file_list; /* list of filepool_entry_t */
    unsigned int   segments;
    char           directory[PATH_MAX];
    unsigned int   crc_fails;
} filepool_t;

typedef struct {
    int  fd;
    char name[PATH_MAX];
    void *map_loc;
    unsigned long file_size;

    struct nzb_file *nzbf;
} filepool_entry_t;

#endif
