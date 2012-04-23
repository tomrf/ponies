/*
 *
 * XXX this file REALLY needs a security audit XXX
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define __USE_GNU
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "common.h"


#define MATCH_SEGMENT  !strncmp("<segment ", buf_ptr, sizeof("<segment ") - 1)
#define MATCH_GROUP    !strncmp("<group>", buf_ptr, sizeof("<group>") - 1)
#define MATCH_FILE     !strncmp("<file ", buf_ptr, sizeof("<file ") - 1)
#define MATCH_FILE_END !strncmp("</file>", buf_ptr, sizeof("</file>") - 1)
#define MATCH_NZB      !strncmp("<nzb ", buf_ptr, sizeof("<nzb ") - 1)
#define MATCH_NZB_END  !strncmp("</nzb>", buf_ptr, sizeof("</nzb>") - 1)

char* get_attr(char *buf, char *key);
char* get_value(char *buf);
void  subject_sanitize(char *s, char *dest);
int   discriminate_par2(const void * foo, const void * bar);

static int html_translate(char *s);

struct nzb *nzbparse(char *path) 
{
    FILE *fp; 
    char buf[1024];
    char *buf_ptr;

    list_t *file_list    = NULL;
    list_t *segment_list = NULL;
    list_t *group_list   = NULL;

    unsigned int total_size = 0, file_size = 0;
    size_t buf_len;
    int par2 = FALSE;
    struct nzb *ret;
    int line = 0;

    int sanity_checks_passed = 0;

    ret = xmalloc(sizeof (struct nzb));
    ret->num_files = 0;
    ret->size = 0;
    memset(ret->path, '\0', sizeof ret->path);


    file_list = list_create();


    if ((fp = fopen(path, "r")) == NULL) {
        DEBUG(1, "fopen(%s) failed: %s", path, strerror(errno)); 
        return ret;
    }


    while (fgets(buf, sizeof buf, fp) != NULL) {

        buf_ptr = buf;
        buf_len = strlen(buf);

        line++;


        /* remove whitespace/tab prefix */
        while (*buf_ptr == ' ' || *buf_ptr == '\t')
            buf_ptr++;

        if (MATCH_NZB || MATCH_NZB_END)
          sanity_checks_passed++;

        if (MATCH_FILE) {
            char *subject;

            subject = get_attr(buf_ptr, "subject");
            assert(subject != NULL);
            par2 = (strcasestr(subject, ".par2") == NULL) ? FALSE : TRUE;
            free(subject); // XXX TEST XXX

            /* primitive parse test */
            if (segment_list != NULL || group_list != NULL) {
                DEBUG(1, "parseerror %s line %d", path, line);
                return NULL;
            }
            segment_list = list_create();
            group_list   = list_create();
        }
        else if (MATCH_FILE_END) {
            struct nzb_file *file;

            file = xmalloc(sizeof (struct nzb_file));

            file->num_segments = segment_list->n;
            file->num_groups   =   group_list->n;
            file->segments = (struct nzb_segment **)list_to_array(segment_list);
            file->groups   = (char **)              list_to_array(group_list);
            file->ispar    = par2;
            file->size     = file_size;

            file_size      = 0;

            list_destroy(segment_list, NULL);
            list_destroy(group_list, NULL);

            segment_list = NULL;
            group_list   = NULL;

            list_push(file_list, file);
        }
        else if (MATCH_GROUP) {
            char *group;
            
            group = get_value(buf_ptr);
            list_push(group_list, group);
        }
        else if (MATCH_SEGMENT) {
            struct nzb_segment *seg;
            char               *num;
            char               *bytes;
            char               *msg_id;

            seg = xmalloc(sizeof (struct nzb_segment));

            bytes  = get_attr(buf_ptr, "bytes");
            num    = get_attr(buf_ptr, "number");
            msg_id = get_value(buf_ptr);

            html_translate(msg_id);

            seg->bytes  = atoi(bytes);
            seg->num    = atoi(num);
            seg->msg_id = msg_id;

            total_size += seg->bytes;
            file_size  += seg->bytes;

            free(num);
            free(bytes);

            list_push(segment_list, seg);
        }
    }

    /* did we pass all sanity checks? if not, free data and return NULL */
    if (sanity_checks_passed != 2) {
      free(ret);
      ret = NULL;
    }

    /* if ret != NULL, everything should be ok */
    if (ret != NULL) {
      strcpy(ret->path, path);
      ret->files     = list_to_array(file_list);
      ret->num_files = file_list->n;
      ret->size      = total_size;
      qsort(ret->files,ret->num_files,sizeof (struct nzb_file *),discriminate_par2);
    }

    list_destroy(file_list, NULL);

    fclose(fp);

    return ret;
}


/*
 * frees all segment- and group-info from a struct nzb
 * total size, errorstatus, number of files and path 
 * is still intact
 */
void nzb_killer(struct nzb *nzb)
{
    int i, j;

    for (i = 0; i < nzb->num_files; ++i) {
        struct nzb_file *f = nzb->files[i];

        for (j = 0; j < f->num_segments; ++j) {
            struct nzb_segment *s = f->segments[j];

            free(s->msg_id);
            free(s);
        }

        for (j = 0; j < f->num_groups; ++j) {
            char *group = f->groups[j];

            free(group);
        }

        free(f->segments);
        free(f->groups);
        free(f);
    }
    free(nzb->files);
}


char *get_attr(char *buf, char *key)
{
    char *eq;
    char *attr_start;
    char *val_end;
    char *val_start;
    int val_len;
    int key_len;
    char delimiter;

    char *retptr;
    char *buf_ptr = buf;


    if ((key_len = strlen(key)) == 0)
        return NULL;


    while ((eq = strchr(buf_ptr, '=')) != NULL) {
        attr_start = eq - key_len;

        delimiter = eq[1];
        if ((delimiter != '\'' && delimiter != '\"') || eq[2] == '\0')
            return NULL; /* should be " or ', return indicating error */

        if (attr_start - 1 < buf_ptr) /* key too short (must start with ' ') */
            goto end;

        if (*(attr_start - 1) != ' ')
            goto end;
        
        if (!strncmp(attr_start, key, key_len)) {
            val_start = eq + 2;
            val_end = strchr(val_start, delimiter);

            if (val_end == NULL)
                return NULL; /* could not find end delimiter */

            val_len = val_end - val_start;

            retptr = xmalloc(val_len + 1);

            strncpy(retptr, val_start, val_len);
            retptr[val_len] = '\0';

            return retptr;
        }

end:
        buf_ptr = eq + 2; /* point to the byte after the delmiter */
        buf_ptr = strchr(buf_ptr, delimiter);/* point to end delimiter or NULL*/
        if (buf_ptr == NULL)
            return NULL;
    }

    return NULL;
}


char* get_value(char *buf)
{
    char *gt;
    char *val_end;
    char *val_start;
    int val_len;

    char *retptr;
    char *buf_ptr = buf;

    gt = strchr(buf_ptr, '>');
    if (gt == NULL)
        return NULL;

    val_start = gt + 1;

    val_end = strchr(val_start, '<');
    if (val_end == NULL)
        return NULL;

    val_len = val_end - val_start;

    retptr = xmalloc(val_len + 1);

    strncpy(retptr, val_start, val_len);
    retptr[val_len] = '\0';

    return retptr;
}

int discriminate_par2(const void * foo, const void * bar)
{
    struct nzb_file *f00 = (struct nzb_file *)*(void **)foo;
    struct nzb_file *b4r = (struct nzb_file *)*(void **)bar;

    if (b4r->ispar == f00->ispar)
        return 0;
    else if (b4r->ispar == TRUE)
        return -1;
    else
        return 1;
}

/*
 * Take a NULL-terminated subject-string in, and put a NULL-terminated string,
 * not bigger than s in dest, representing the filename 
 */
void subject_sanitize(char *s, char *dest)
{ 
    char *lastdot;
    char *end_tok, *start_tok;
    char tok;


    lastdot = strrchr(s, '.');
    if (lastdot == NULL)
        goto copy_old_str;

    end_tok = strpbrk(lastdot, " \'\"&[(#");
    if (end_tok == NULL)
        goto copy_old_str;

    tok = *end_tok;
    *end_tok = '\0';
    switch (tok) {
        case   0: strcpy(dest, s); return;
        case '[':       tok = ']'; break;
        case '(':       tok = ')'; break;
        case '&':       tok = ';'; break;
        case ' ':                  goto find_dash;
    }

    start_tok = strrchr(s, tok);

    if (start_tok == NULL) {

find_dash:
        start_tok = strrchr(s, '-');
        if (start_tok == NULL || ++start_tok == NULL)
            goto copy_old_str;
    }
    strcpy(dest, start_tok + 1);
    return;

copy_old_str:
    strcpy(dest, s);
}

unsigned int nzb_segments_total(struct nzb *nzb)
{
    int sum = 0;
    int i;

    for (i = 0; i < nzb->num_files; ++i)
        sum += nzb->files[i]->num_segments;

    return sum;
}


int html_translate(char *s)
{
  char   *ptr = s;
  int    ret = 0;

  while (*ptr) {

    /* entity names */
    if      (!strncmp(ptr, "&amp;", 5))  {  *s++ = '&';   ptr += 5;   ret++;  }
    else if (!strncmp(ptr, "$quot;", 6)) {  *s++ = '"';   ptr += 6;   ret++;  }
    else if (!strncmp(ptr, "$apos;", 6)) {  *s++ = '\'';  ptr += 6;   ret++;  }
    else if (!strncmp(ptr, "$lt;", 4))   {  *s++ = '<';   ptr += 4;   ret++;  }
    else if (!strncmp(ptr, "$gt;", 4))   {  *s++ = '>';   ptr += 4;   ret++;  }

    /* entity number */
    else if (!strncmp(ptr, "&#34;", 5))  {  *s++ = '"';   ptr += 5;   ret++;  }
    else if (!strncmp(ptr, "&#39;", 5))  {  *s++ = '\'';  ptr += 5;   ret++;  }
    else if (!strncmp(ptr, "&#38;", 5))  {  *s++ = '&';   ptr += 5;   ret++;  }
    else if (!strncmp(ptr, "&#60;", 5))  {  *s++ = '<';   ptr += 5;   ret++;  }
    else if (!strncmp(ptr, "&#62;", 5))  {  *s++ = '>';   ptr += 5;   ret++;  }

    /* default */
    else *s++ = *ptr++;

  }

  *s = '\0';

  return ret;
}
