#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "crc32.h"

u_int32_t crc_tab[256];

u_int32_t chksum_crc32 (unsigned char *block, unsigned int length)
{
   register unsigned long crc;
   unsigned long i;

   crc = 0xFFFFFFFF;
   for (i = 0; i < length; i++)
   {
      crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];
   }
   return (crc ^ 0xFFFFFFFF);
}

void chksum_crc32gentab(void)
{
   unsigned long crc, poly;
   int i, j;

   poly = 0xEDB88320L;
   for (i = 0; i < 256; i++)
   {
      crc = i;
      for (j = 8; j > 0; j--)
      {
         if (crc & 1)
         {
            crc = (crc >> 1) ^ poly;
         }
         else
         {
            crc >>= 1;
         }
      }
      crc_tab[i] = crc;
   }
}
