#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "common.h"


int x_atoi(const char *nptr)
{
  if (nptr != NULL)
    return atoi(nptr);
  
  return -1;
}

long x_atol(const char *nptr)
{
  if (nptr != NULL)
    return atol(nptr);
  
  return 0;
}


int yinf_populate_struct(yenc_info_t *yenc_info, char *data, size_t len)
{
  char   *ptr;

  memset(yenc_info->name, '\0', sizeof yenc_info->name);

  if ((ptr = yinf_get_key_value(yinf_find_key_offset(data, "name="), 1)) != NULL) {
    snprintf(yenc_info->name, sizeof yenc_info->name, "%s", ptr);
    free(ptr);
  }

  ptr = yinf_get_key_value(yinf_find_key_offset(data, "line="), 0);
  yenc_info->line = x_atoi(ptr);
  if (ptr != NULL) free(ptr);

  ptr = yinf_get_key_value(yinf_find_key_offset(data, "size="), 0);
  yenc_info->size = x_atol(ptr);
  if (ptr != NULL) free(ptr);

  if (yenc_info->has_parts == TRUE) {

    /* part */
    ptr = yinf_get_key_value(yinf_find_key_offset(data, "part="), 0);
    yenc_info->part = x_atoi(ptr);
    if (ptr != NULL) free(ptr);

    /* total */
    ptr = yinf_get_key_value(yinf_find_key_offset(data, "total="), 0);
    yenc_info->total = x_atoi(ptr);
    if (ptr != NULL) free(ptr);

    /* begin */
    ptr = yinf_get_key_value(yinf_find_key_offset(data, "begin="), 0);
    yenc_info->part_begin = x_atol(ptr);
    if (ptr != NULL) free(ptr);

    /* end */
    ptr = yinf_get_key_value(yinf_find_key_offset(data, "end="), 0);
    yenc_info->part_end = x_atol(ptr);
    if (ptr != NULL) free(ptr);

  } else {

    /* has no part */
    yenc_info->part = 0;
    yenc_info->total = 0;
    yenc_info->part_begin = 1;
    yenc_info->part_end = yenc_info->data_size;

  }

  /* crc32 */ // XXX: commented out, we don't need this, at least not yet
  //if ((ptr = yinf_get_key_value(yinf_find_key_offset_backwards(data, "crc32="), 0)) != NULL) {
  //  snprintf(yenc_info->crc32, sizeof yenc_info->crc32, "%s", ptr);
  //  free(ptr);
  //}

  /* pcrc32 */
  if (yenc_info->has_parts == TRUE) {
    if ((ptr = yinf_get_key_value(yinf_find_key_offset_backwards(data, "pcrc32="), 0)) != NULL) {
      memset(yenc_info->pcrc32, '0', 8 - strlen(ptr));
      strcat(yenc_info->pcrc32, ptr); //XXX: OVERFLOW_X
      free(ptr);
    }
  }

  return 0;
}


char* yinf_get_key_value(char *s, int ignore_space)
{
  char       *ptr;
  char       *new;

  int         new_len = 64 * 2;
  int         do_copy = 0;
  int         copied  = 0;

  int         i; 


  if (s == NULL)
    return NULL;

  new = xmalloc(new_len);
  memset(new, '\0', new_len);

  ptr = s;

  for (i = 0; ; i++) {

    if (*ptr == '\0' || *ptr == '\n' || *ptr == '\r')
      break;

    if (*ptr == ' ' && ignore_space == 0)
      break;

    if (do_copy == 1) {
      new[copied] = *ptr;
      new[++copied] = '\0';
      if (copied >= (new_len - 1)) {
        new_len += 64 * 2;
        new = realloc(new, new_len);
        assert(new != NULL);
      }
    }
      
    if (*ptr == '=')
      do_copy = 1;

    ptr++;
  }

  if (copied > 0)
    return new;

  free(new);

  return NULL;
}


char* yinf_find_key_offset(char *s, char *key) // XXX: this could be a little slow...
{
  char      *ptr;
  int       len_s;
  int       len_key;
  int       i;

  len_s = strlen(s);
  len_key = strlen(key);

  for (i = 0; i < len_s - len_key; i++) {
    ptr = s + i;
    if (!strncmp(key, ptr, len_key) && *(ptr-1) == ' ')
      return ptr;
  }

  return NULL;
}


char* yinf_find_key_offset_backwards(char *s, char *key)
{
  char      *ptr;
  int       len_s;
  int       len_key;
  int       i;

  len_s = strlen(s);
  len_key = strlen(key);

  for (i = len_s - len_key; i > 0; i--) {
    ptr = s + i;
    if (!strncmp(key, ptr, len_key) && *(ptr-1) == ' ')
      return ptr;
  }

  return NULL;
}


void yinf_debug_print_yenc_info(yenc_info_t *yenc_info)
{
  printf("yinf_debug_print_yenc_info: printing yenc_info @ %p\n", yenc_info);  
  printf("  -- ybegin --\n");
  printf("  part:          %d\n", yenc_info->part);
  printf("  total:         %d\n", yenc_info->total);
  printf("  line:          %d\n", yenc_info->line);
  printf("  size:          %d\n", yenc_info->size);
  printf("  name:          %s\n", yenc_info->name);
  printf("  -- ypart --\n");
  printf("  part begin:    %ld\n", yenc_info->part_begin);
  printf("  part end:      %ld\n", yenc_info->part_end);
  printf("  -- yend --\n");
  printf("  crc32:         %s\n", yenc_info->crc32);
  printf("  pcrc32:        %s\n", yenc_info->pcrc32);
  printf("  -- calculated --\n");
  printf("  has parts:     %d\n", yenc_info->has_parts);
  printf("  data:          %p\n", yenc_info->data);
  printf("  data size:     %d\n", yenc_info->data_size);
  printf("  expected size: %ld (diff: %ld)\n", yenc_info->part_end - yenc_info->part_begin,
                    yenc_info->data_size - (yenc_info->part_end - yenc_info->part_begin));

  printf("  crc32_ok:      %d\n", yenc_info->crc32_ok);
  printf("  pcrc32_ok:     %d\n", yenc_info->pcrc32_ok);
}
