#ifndef _CRC32_H
#define _CRC32_H

u_int32_t chksum_crc32 (unsigned char *block, unsigned int length);
void chksum_crc32gentab(void);

#endif
