/*	$OpenBSD: syncicache.c,v 1.4 2022/08/29 02:01:18 jsg Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (C) 1995-1997, 1999 Wolfgang Solfrank.
 * Copyright (C) 1995-1997, 1999 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: syncicache.c,v 1.2 1999/05/05 12:36:40 tsubai Exp $
 */

#include <sys/param.h>

#include <machine/cpufunc.h>

void
__syncicache(void *from, size_t len)
{
#if 0
	size_t	l, off;
	char	*p;

	off = (uintptr_t)from & (cacheline_size - 1);
	l = len += off;
	p = (char *)from - off;

	do {
		__asm volatile ("dcbst 0,%0" :: "r"(p));
		p += cacheline_size;
		l -= cacheline_size;
	} while (l + cacheline_size > cacheline_size);
	__asm volatile ("sync");
	p = (char *)from - off;
	do {
		__asm volatile ("icbi 0,%0" :: "r"(p));
		p += cacheline_size;
		len -= cacheline_size;
	} while (len + cacheline_size > cacheline_size);
	__asm volatile ("sync; isync");
#else
	sync();
	__asm volatile ("icbi 0,%0" :: "b"(from));
	isync();
#endif
}

