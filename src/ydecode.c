#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "common.h"


yenc_info_t* ydecode_buffer(char *in, size_t len)
{
  yenc_info_t   *yenc_info;

  char          *decoded_data;
  size_t        decoded_data_len;

  int           offset_start;
  int           offset_end;
  int           i, j, c;

  char          crc32[9];

  j = 0;

  while (*in == '\n' || *in == '\r') { in++; };

  yenc_info = xmalloc(sizeof (yenc_info_t));

  offset_start = yenc_find_data_offset_start(in, len);
  offset_end = yenc_find_data_offset_end(in, len);

  if (offset_start == -1 || offset_end == -1) {
    free(yenc_info);
    return NULL;
  }

  decoded_data_len = len;
  decoded_data = xmalloc(decoded_data_len);

  for (i = offset_start; i < offset_end; i++) {

    if (!strncmp(in+i, "\n..", 3))
      i += 2;

    c = in[i];
    
    if (c == '\n' || c == '\r')
      continue;

    if (c == '=') {
        
      i++; 
      while (in[i] == '\r' || in[i] == '\n') {
        if (!strncmp(in+i, "\n..", 3))
          i += 2;
        else 
          i++;
      }

      c = in[i];
      c = c - 42 - 64 % 256;
    }
    else {
      c = c - 42 % 256;
    }

    decoded_data[j++] = c;

    if (j >= (decoded_data_len - 1)) {
      decoded_data_len += 4096;
      decoded_data = realloc(decoded_data, decoded_data_len);
      assert(decoded_data != NULL);
    }

  } /* for */

  yenc_info->data = decoded_data;
  yenc_info->data_size = j;
  yenc_info->has_parts = yenc_has_ypart(in, 256);

  memset(yenc_info->crc32, '\0', sizeof yenc_info->crc32);
  memset(yenc_info->pcrc32, '\0', sizeof yenc_info->pcrc32);
  yenc_info->crc32_ok = FALSE;
  yenc_info->pcrc32_ok = FALSE;

  yinf_populate_struct(yenc_info, in, len);

  /* check part crc (pcrc32) if we have any */
  if (yenc_info->pcrc32) {
    snprintf(crc32, sizeof crc32, "%08x", chksum_crc32((unsigned char *)yenc_info->data, yenc_info->data_size));
    if (!strcasecmp(yenc_info->pcrc32, crc32))
      yenc_info->pcrc32_ok = TRUE;
  } 

  return yenc_info;
}


int yenc_find_data_offset_start(char *buf, size_t buf_len)
{
  int i;
  int lnc;
  int lnc_expect;

  lnc = 0;
  lnc_expect = 0;

  if (yenc_has_ybegin(buf, 8) == TRUE)
    lnc_expect++;

  if (yenc_has_ypart(buf, 256) == TRUE)
    lnc_expect++;

  for (i = 0; i < buf_len; i++) {
    if (buf[i] == '\n')
      lnc++;
    if (lnc == lnc_expect)
      return i;
  }
 
  return -1;
}


int yenc_find_data_offset_end(char *buf, size_t buf_len)
{
  int i;

  i = yenc_find_key(buf, "=yend", buf_len, NULL, 0);

  return i;
}


int yenc_has_ybegin(char *buf, size_t len)
{
  if (yenc_find_key(buf, "=ybegin", len, NULL, 0) == 0)
    return TRUE;

  return FALSE;
}


int yenc_has_ypart(char *buf, size_t len)
{
  if (yenc_find_key(buf, "=ypart", len, NULL, 0) != -1)
    return TRUE;

  return FALSE;
}

//XXX size_t
int yenc_find_key(char *buf, const char *key, size_t buf_len, char *value, size_t value_len)
{
  int i, j;
  int tstart, tstop, tstep;

  int key_len;

  char *ptr;
  char *ptr_end;

  unsigned int _vlen;

  key_len = strlen(key);

  if (buf_len < 500) {
    tstart = 0;
    tstop = buf_len;
    tstep = 1;
  } else {
    tstart = buf_len;
    tstop = 0;
    tstep = -1;
  }
   
  i = tstart;
  for (;; i += tstep) {

    if (tstep == 1 && i >= tstop)
      break;

    if (tstep == -1 && i <= tstop)
      break;

    ptr = buf + i;

    if (!strncmp(ptr, key, key_len)) {

      if (value != NULL) {

        for (j = key_len; j < 512; j++) {
          ptr_end = ptr + j;
          if (*ptr_end == ' ' || *ptr_end == '\n')
            break;
        }

        _vlen = j - key_len;

        if (_vlen > value_len)
          _vlen = value_len;
        
        memset(value, '\0', value_len);

        strncpy(value, ptr + key_len, _vlen);

      }

      return i;
 
    } /* strncmp */

  } /* for */

  return -1;
}
