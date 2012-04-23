#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/stat.h>
#include "common.h"
#include "filepool.h"


static filepool_entry_t *
find_entry(filepool_t *pool, struct nzb_file *nzbf);
static filepool_entry_t *
prepare_file(filepool_t *pool, worker_job_t *job);
static int 
open_file(filepool_t *pool, const char *filename);
static void *
map_file(int fd, unsigned long size);
static struct nzb_segment *
find_segment_with_status(struct nzb_file *nzbf, int status);
static struct nzb_file *
find_incomplete_file(filepool_t *pool);
static int
count_segments_with_status(struct nzb *nzb, int status);
static void
close_file(filepool_t *pool, worker_job_t *job, filepool_entry_t *ent);
static void
mark_file_as_done(filepool_t *pool, struct nzb_file *nzbf);



/*
 * We need a way to:
 * destroy pool 
 * close files
 * update segmentcount
 * get stats'n'shit?
 *
 * XXX: lock pool when counting stuff?
 */
 

filepool_t *
filepool_init(struct queue_node *qnode)
{
    unsigned int i, j;

    filepool_t *pool;
    pool = xmalloc(sizeof (filepool_t));

    pool->qnode        = qnode;
    pool->file_list    = list_create();
    pool->segments     = 0;
    pool->crc_fails    = 0;
    pool->directory[0] = '\0';


    for (i = 0; i < qnode->nzb->num_files; ++i) {
        struct nzb_file *nzbf = qnode->nzb->files[i];

        nzbf->segments_completed = 0;
        nzbf->segments_assigned  = 0;
        nzbf->segments_failed    = 0;

        for (j = 0; j < nzbf->num_segments; ++j)
            nzbf->segments[j]->status = SEGSTAT_FREE;

        pool->segments += nzbf->num_segments;
    }

    /* register with a global list? */
    return pool;
}


/*
 * find a free segment and return a job or NULL
 */
worker_job_t *
filepool_get_job(filepool_t *pool)
{
    worker_job_t *job;
    struct nzb_file *nzbf;
    struct nzb_segment *nzbfs;

    list_lock(pool->file_list); 

    job = NULL;

    /* find free segment */
    nzbf = find_incomplete_file(pool);
    if (nzbf == NULL)
        goto end;
    nzbfs = find_segment_with_status(nzbf, SEGSTAT_FREE);
    if (nzbfs == NULL) /* racecondition when skipping files fix */
        goto end;

    /* setup job */
    job = xmalloc(sizeof (worker_job_t));
    job->nzbf     = nzbf;
    job->nzbfs    = nzbfs;
    job->yi       = NULL;
    job->status   = 0;//JSTAT_DOWNLOADING;

    nzbf->segments_assigned++;
    nzbfs->status = SEGSTAT_ACTIVE;

end:
    list_unlock(pool->file_list);

    return job;
}

void *
filepool_map_loc(filepool_t *pool, worker_job_t *job)
{
    filepool_entry_t *ent;

    /* is the file already opened? */
    ent = find_entry(pool, job->nzbf);
    if (ent != NULL)
        return ent->map_loc;

    /* this is a new file */
    ent = prepare_file(pool, job);
    if (ent == NULL)
        return NULL;
    
    /* NULL if something failed XXX: segfault if ent==NULL */
    return ent->map_loc;
}


/*
 *
 * This function opens a new file and pushes an entry to the pools file_list
 * (path, mmap-position, filesize, nzbf)
 *
 * It fails if the disk is full or the new file cannot be opened or mmaped.
 *
 */
static filepool_entry_t * prepare_file(filepool_t *pool, worker_job_t *job) {
    filepool_entry_t *ent = xmalloc(sizeof(filepool_entry_t));


    /* Skip downloading of par2-files if all files was correct. */
    if (global_opts.skip_par2 == TRUE
        && pool->crc_fails == 0 
        && strend(job->yi->name, ".par2"))
        goto exists;


    strncpy(ent->name, job->yi->name, sizeof ent->name);
    ent->file_size = job->yi->size;
    ent->nzbf      = job->nzbf;

    ent->fd = open_file(pool, ent->name);
    if (ent->fd == -1) 
        goto error;
    else if (ent->fd == -2)
        goto exists;
    if (ftruncate(ent->fd, ent->file_size) == -1) 
        goto error;
    ent->map_loc = map_file(ent->fd, ent->file_size);
    if (ent->map_loc == NULL)
        goto error;

    /* XXX: need to get file_num and segments_total
     * resume-logic here? modify struct nzb? 
     * tell segment pool /dispatcher what is going on? */

    /* XXX2: open example.p0ny and rename to example when
     * all segments are complete or failed? */

    list_lock(pool->file_list);
    list_push(pool->file_list, ent);
    list_unlock(pool->file_list);

    return ent;

exists:
    mark_file_as_done(pool, job->nzbf); 
    printf("\r * skipping completed file: %s\n", ent->name);
error:
    if (ent->fd > -1)
        close(ent->fd);
    free(ent);
    /* XXX: freeze dispatcher and report? */
    return NULL;
}


static filepool_entry_t *
find_entry(filepool_t *pool, struct nzb_file *nzbf)
{
    node_t *node;

    list_lock(pool->file_list);
    node = pool->file_list->head;
    while (node != NULL) {
        filepool_entry_t *ent = node->data;

        if (ent->nzbf == nzbf) {
            list_unlock(pool->file_list);
            return ent;
        }

        node = node->next;
    }
    list_unlock(pool->file_list);

    return NULL;
}


static int 
open_file(filepool_t *pool, const char *filename)
{
    int fd;
    struct stat s;
    char path[PATH_MAX];

    snprintf(path, sizeof path, "%s/%s", pool->directory, filename);
    if (stat(path, &s) == 0)
        return -2;

    snprintf(path, sizeof path, "%s/%s.p0ny", pool->directory, filename);

    fd = open(path, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd == -1) {
        DEBUG(0, "could not open file '%s': %s", path, strerror(errno)); 
    } else {
        DEBUG(3, "opened file '%s' (fd=%d)", path, fd);
    }
    return fd;
}


static void *
map_file(int fd, unsigned long size)
{  
    void *location;

    location = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (location == MAP_FAILED) {
        DEBUG(0, "could not mmap file %d: %s", fd, strerror(errno)); 
        return NULL;
    } else {
        DEBUG(3, "mapped file @ %p (fd=%d, size=%u)", location, fd, size);
    }

    return location;
}

static struct nzb_file *
find_incomplete_file(filepool_t *pool)
{
    int i;

    list_lock(pool->file_list);
    for (i = 0; i < pool->qnode->nzb->num_files; ++i) {
        struct nzb_file *nzbf = pool->qnode->nzb->files[i];
        int x = 0;

        x += nzbf->segments_completed;
        x += nzbf->segments_failed;
        x += nzbf->segments_assigned;

        if (x < nzbf->num_segments)
            return nzbf;
    }
    list_unlock(pool->file_list);

    return NULL;
}

static struct nzb_segment *
find_segment_with_status(struct nzb_file *nzbf, int status)
{
    int i;

    for (i = 0; i < nzbf->num_segments; ++i) {
        struct nzb_segment *nzbfs = nzbf->segments[i];

        if (nzbfs->status == status)
            return nzbfs;
    }

    return NULL;
}

static int
count_segments_with_status(struct nzb *nzb, int status)
{
    int i, j, ret = 0;

    for (i = 0; i < nzb->num_files; ++i) {
        struct nzb_file *nzbf = nzb->files[i];

        for (j = 0; j < nzbf->num_segments; ++j) {
            if (nzbf->segments[j]->status == status)
                ret++;
        }
    }

    return ret;
}

void pool_print(filepool_t *pool)
{
    node_t *node;
    int i = 0;

    list_lock(pool->file_list);
    puts("Open File Entries: "); 
    node = pool->file_list->head;
    while (node != NULL) {
        filepool_entry_t *ent = node->data;

        printf(" - Entry: %d\n", ++i);
        printf("   * fd        = %d\n", ent->fd);
        printf("   * map_loc   = %p\n", ent->map_loc);
        printf("   * file_size = %lu\n", ent->file_size);
        printf("   * filename  = '%s'\n", ent->name);
        printf("   * nzbf      = '%p'\n", ent->nzbf);

    }
    puts("---");
    list_unlock(pool->file_list);   
}

/*
 * returns >0 if all segments is complete or failed
 */
int filepool_incomplete(filepool_t *pool)
{
    int free, active;

    list_lock(pool->file_list);
    free   = count_segments_with_status(pool->qnode->nzb, SEGSTAT_FREE);
    active = count_segments_with_status(pool->qnode->nzb, SEGSTAT_ACTIVE);
    list_unlock(pool->file_list);

    return free+active;
}

/*
 * returns number of free segments
 */
int filepool_free_segments(filepool_t *pool)
{
    list_lock(pool->file_list);
    return count_segments_with_status(pool->qnode->nzb, SEGSTAT_FREE);
    list_unlock(pool->file_list);
}

/*
 * assumes a sane segment that can be written to disk
 */
void 
filepool_segcomplete(filepool_t *pool, worker_job_t *job)
{
    void *dest;

    list_lock(pool->file_list);

    if (pool->directory[0] == '\0') {
        snprintf(pool->directory, sizeof pool->directory, "%s/%s",
            global_opts.dir_download, basename(pool->qnode->nzb->path)); 
        if (! strcasecmp(pool->directory+strlen(pool->directory)-4, ".nzb"))
            pool->directory[strlen(pool->directory)-4] = '\0';
        if (mkdir(pool->directory, S_IRUSR|S_IWUSR|S_IXUSR) == -1 && errno != EEXIST)
            die("Couldn't create download directory for nzb (%s)", strerror(errno));
    }

    if (chk_disk_full(global_opts.dir_download)) {
        /* 
         * The disk is full. 
         *
         * 1) Pause the dispatcher associated with this qnode to paused.
         *
         * 2) Since we already have downloaded a segment that is to be
         * discarded, this segment must be reset.
         *
         * 3) set global pause
         */
        pool->qnode->queue_status = QSTAT_PAUSED;
        job->nzbf->segments_assigned--;
        job->nzbfs->status = SEGSTAT_FREE;
        sqw_set_runtime_key("diskfull", "yes");

        INFO(1, "DISK IS FULL! --- SETTING DISKFULL");
        
        goto end;
    }


    dest = filepool_map_loc(pool, job);
    if (dest == NULL) /* skipped file, dont need to close or set status */
        goto end;

    assert(job->yi->part_begin+job->yi->data_size-1 <= find_entry(pool, job->nzbf)->file_size);
    memcpy(dest+job->yi->part_begin-1, job->yi->data, job->yi->data_size);

    job->nzbf->segments_completed++;
    job->nzbf->segments_assigned--;
    job->nzbfs->status = SEGSTAT_COMPLETED;

    if (job->crc_failed == TRUE)
        pool->crc_fails ++;

    if (find_segment_with_status(job->nzbf, SEGSTAT_FREE) == NULL
        && find_segment_with_status(job->nzbf, SEGSTAT_ACTIVE) == NULL) {
        close_file(pool, job, find_entry(pool, job->nzbf));
    }

end:
    free(job->yi->data);
    free(job->yi);
    free(job);

    list_unlock(pool->file_list);
}

void filepool_segfail(filepool_t *pool, void* ptr)
{

}

static void
close_file(filepool_t *pool, worker_job_t *job, filepool_entry_t *ent)
{
    char path1[PATH_MAX];
    char path2[PATH_MAX];

    /*
     * fix pool entry
     * log / spam to terminal
     * update coutns
     * mark file as done?
     * close
     */

    if (munmap(ent->map_loc, ent->file_size))
        assert(!errno);
    if (close(ent->fd) == -1)
        assert(!errno);

    snprintf(path1, sizeof path1, "%s/%s.p0ny", pool->directory, ent->name);
    snprintf(path2, sizeof path2, "%s/%s", pool->directory, ent->name);
    assert(rename(path1, path2) == 0);
    snprintf(path1, sizeof path1, "%s.p0ny", ent->name);
    snprintf(path2, sizeof path2, "%s", ent->name);
    //printf("Renamed completed file %s=>%s\n", path1,path2);
}

static void
mark_file_as_done(filepool_t *pool, struct nzb_file *nzbf)
{
    int i;
    list_lock(pool->file_list);
    for (i = 0; i < nzbf->num_segments; ++i) {
        if (nzbf->segments[i]->status == SEGSTAT_FREE) {
            nzbf->segments[i]->status = SEGSTAT_COMPLETED;
            nzbf->segments_completed++;
        }
        else if  (nzbf->segments[i]->status == SEGSTAT_ACTIVE) {
            nzbf->segments[i]->status = SEGSTAT_COMPLETED;
            nzbf->segments_completed++;
            nzbf->segments_assigned--;
        }
    }
    list_unlock(pool->file_list);
}
