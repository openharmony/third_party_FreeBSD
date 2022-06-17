/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FS_UTIL_H
#define _FS_UTIL_H

#include <stdint.h>

#define pwarn  printf
#define pfatal printf
#define perr   printf


/*
 * le16dec(),le32dec(),le16enc(),le32enc()
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/contrib/elftoolchain/libpe/_libpe.h
 */
/* Encode/Decode macros */
#if defined(ELFTC_NEED_BYTEORDER_EXTENSIONS)
static  __inline uint16_t
le16dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[1] << 8) | p[0]);
}

static __inline uint32_t
le32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static __inline void
le16enc(void *pp, uint16_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

static __inline void
le32enc(void *pp, uint32_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}
#endif	/* ELFTC_NEED_BYTEORDER_EXTENSIONS */

/*
 * roundup2
 * Link： https://github.com/freebsd/freebsd-src/
 * Path:  freebsd-src/tools/build/cross-build/include/common/sys/cdefs.h
 */
#ifndef roundup2
#define roundup2(x, y) \
	(((x) + ((y)-1)) & (~((y)-1))) /* if y is powers of two */
#endif
#endif // _FS_UTIL_H