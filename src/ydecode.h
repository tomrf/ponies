#ifndef YDECODE_H
#define YDECODE_H

typedef struct yenc_info {

  /* ybegin */
  unsigned int     part;
  unsigned int     total;
  unsigned int     line;
  ssize_t          size;
  char             name[PATH_MAX];

  /* ypart */
  off_t            part_begin;
  off_t            part_end;

  /* yend */
  char             crc32[9];
  char             pcrc32[9];

  /* other / own */
  char             *data;       /* pointer to decoded data buffer */
  ssize_t          data_size;   /* size of decoded data */
  int              has_parts;   /* TRUE / FALSE, is the data part of a bigger file? */
  int              crc32_ok;    /* TRUE / FALSE, does crc32 sum match? */
  int              pcrc32_ok;   /* TRUE / FALSE, does part pcrc32 (crc32 of data part) match? */

} yenc_info_t;


#endif

