/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef HTS_SHA1_H
#define HTS_SHA1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern const int hts_sha1_size;

struct HTSSHA1;

void hts_sha1_init(struct HTSSHA1* context);
void hts_sha1_update(struct HTSSHA1* context, const uint8_t* data, unsigned int len);
void hts_sha1_final(struct HTSSHA1* context, uint8_t digest[20]);

#ifdef __cplusplus
}
#endif

#endif /* HTS_SHA1_H */
