/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 2002 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FPMATH_H_
#define _FPMATH_H_

#include <endian.h>
#ifndef LD128_ENABLE
#include "_fpmath.h"
#endif

#ifndef _IEEE_WORD_ORDER
#define	_IEEE_WORD_ORDER	__BYTE_ORDER
#endif

#ifdef LD128_ENABLE
union IEEEl2bits {
	long double	e;
	struct {
		unsigned long	manl	: 64;
		unsigned long	manh	: 48;
		unsigned int	exp	: 15;
		unsigned int	sign	: 1;
	} bits;
	struct {
		unsigned long	manl	: 64;
		unsigned long	manh	: 48;
		unsigned int	expsign	: 16;
	} xbits;
};

#define	LDBL_NBIT	0
#define	LDBL_IMPLICIT_NBIT
#define	mask_nbit_l(u)	((void)0)

#define	LDBL_MANH_SIZE	48
#define	LDBL_MANL_SIZE	64

#define	LDBL_TO_ARRAY32(u, a) do {			\
	(a)[0] = (uint32_t)(u).bits.manl;		\
	(a)[1] = (uint32_t)((u).bits.manl >> 32);	\
	(a)[2] = (uint32_t)(u).bits.manh;		\
	(a)[3] = (uint32_t)((u).bits.manh >> 32);	\
} while (0)
#else
union IEEEf2bits {
	float	f;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		unsigned int	man	: 23;
		unsigned int	exp	: 8;
		unsigned int	sign	: 1;
#else /* __BIG_ENDIAN */
		unsigned int	sign	: 1;
		unsigned int	exp	: 8;
		unsigned int	man	: 23;
#endif
	} bits;
};

#define	DBL_MANH_SIZE	20
#define	DBL_MANL_SIZE	32

union IEEEd2bits {
	double	d;
	struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
#if _IEEE_WORD_ORDER == __LITTLE_ENDIAN
		unsigned int	manl	: 32;
#endif
		unsigned int	manh	: 20;
		unsigned int	exp	: 11;
		unsigned int	sign	: 1;
#if _IEEE_WORD_ORDER == __BIG_ENDIAN
		unsigned int	manl	: 32;
#endif
#else /* __BIG_ENDIAN */
		unsigned int	sign	: 1;
		unsigned int	exp	: 11;
		unsigned int	manh	: 20;
		unsigned int	manl	: 32;
#endif
	} bits;
};
#endif
#endif /* !_FPMATH_H */
