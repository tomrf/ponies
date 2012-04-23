#ifndef NZB_H
#define NZB_H

#define SEGSTAT_FREE      0
#define SEGSTAT_ACTIVE    1
#define SEGSTAT_COMPLETED 2
#define SEGSTAT_FAILED    3

struct nzb {
    char                path[PATH_MAX];
    unsigned int        size;
    unsigned int        num_files;
    struct nzb_file     **files;
};

struct nzb_file {
    unsigned int        num_groups;
    char                **groups;
    unsigned int        num_segments;
    struct nzb_segment  **segments;
    int                 ispar;
    unsigned long       size;

    /* for filepool */
    int                 segments_completed;
    int                 segments_assigned;
    int                 segments_failed;

};

struct nzb_segment {
    unsigned int        num;
    unsigned int        bytes;
    char                *msg_id; 
    
    /* for filepool */
    int                 status;
};

/* malloced: files, groups, segments, msg_id */

#endif
