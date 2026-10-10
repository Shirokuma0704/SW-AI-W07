/* 이 파일은 MIT의 6.828 과목에서 쓰인 소스 코드에서
   파생되었어요. 원래의 저작권 고지는 전문 그대로
   아래에 실려 있어요. */

/*
 * Copyright (C) 1997 Massachusetts Institute of Technology
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#ifndef THREADS_IO_H
#define THREADS_IO_H

#include <stddef.h>
#include <stdint.h>

/* PORT에서 바이트 하나를 읽어서 반환해요. */
static inline uint8_t
inb (uint16_t port) {
	/* [IA32-v2a] "IN"을 보세요. */
	uint8_t data;
	asm volatile ("inb %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

/* PORT에서 CNT바이트를 차례대로 읽어서 ADDR에서 시작하는
   버퍼에 저장해요. */
static inline void
insb (uint16_t port, void *addr, size_t cnt) {
	/* [IA32-v2a] "INS"를 보세요. */
	asm volatile ("cld; repne; insb"
			: "=D" (addr), "=c" (cnt)
			: "d" (port), "0" (addr), "1" (cnt)
			: "memory", "cc");
}

/* PORT에서 16비트를 읽어서 반환해요. */
static inline uint16_t
inw (uint16_t port) {
	uint16_t data;
	/* [IA32-v2a] "IN"을 보세요. */
	asm volatile ("inw %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

/* PORT에서 CNT개의 16비트(하프워드) 단위를 차례대로 읽어서
   ADDR에서 시작하는 버퍼에 저장해요. */
static inline void
insw (uint16_t port, void *addr, size_t cnt) {
	/* [IA32-v2a] "INS"를 보세요. */
	asm volatile ("cld; repne; insw"
			: "=D" (addr), "=c" (cnt)
			: "d" (port), "0" (addr), "1" (cnt)
			: "memory", "cc");
}

/* PORT에서 32비트를 읽어서 반환해요. */
static inline uint32_t
inl (uint16_t port) {
	/* [IA32-v2a] "IN"을 보세요. */
	uint32_t data;
	asm volatile ("inl %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

/* PORT에서 CNT개의 32비트(워드) 단위를 차례대로 읽어서
   ADDR에서 시작하는 버퍼에 저장해요. */
static inline void
insl (uint16_t port, void *addr, size_t cnt) {
	/* [IA32-v2a] "INS"를 보세요. */
	asm volatile ("cld; repne; insl"
			: "=D" (addr), "=c" (cnt)
			: "d" (port), "0" (addr), "1" (cnt)
			: "memory", "cc");
}

/* PORT에 바이트 DATA를 써요. */
static inline void
outb (uint16_t port, uint8_t data) {
	/* [IA32-v2b] "OUT"을 보세요. */
	asm volatile ("outb %0,%w1" : : "a" (data), "d" (port));
}

/* ADDR에서 시작하는 CNT바이트 버퍼의 각 바이트를
   PORT에 써요. */
static inline void
outsb (uint16_t port, const void *addr, size_t cnt) {
	/* [IA32-v2b] "OUTS"를 보세요. */
	asm volatile ("cld; repne; outsb"
			: "=S" (addr), "=c" (cnt)
			: "d" (port), "0" (addr), "1" (cnt)
			: "cc");
}

/* PORT에 16비트 DATA를 써요. */
static inline void
outw (uint16_t port, uint16_t data) {
	/* [IA32-v2b] "OUT"을 보세요. */
	asm volatile ("outw %0,%w1" : : "a" (data), "d" (port));
}

/* ADDR에서 시작하는 CNT하프워드 버퍼의 각 16비트(하프워드)
   단위 데이터를 PORT에 써요. */
static inline void
outsw (uint16_t port, const void *addr, size_t cnt) {
	/* [IA32-v2b] "OUTS"를 보세요. */
	asm volatile ("cld; repne; outsw"
			: "=S" (addr), "=c" (cnt)
			: "d" (port), "0" (addr), "1" (cnt)
			: "cc");
}

/* PORT에 32비트 DATA를 써요. */
static inline void
outl (uint16_t port, uint32_t data) {
	/* [IA32-v2b] "OUT"을 보세요. */
	asm volatile ("outl %0,%w1" : : "a" (data), "d" (port));
}

/* ADDR에서 시작하는 CNT워드 버퍼의 각 32비트(워드) 단위
   데이터를 PORT에 써요. */
static inline void
outsl (uint16_t port, const void *addr, size_t cnt) {
	/* [IA32-v2b] "OUTS"를 보세요. */
	asm volatile ("cld; repne; outsl"
			: "=S" (addr), "=c" (cnt)
			: "d" (port), "0" (addr), "1" (cnt)
			: "cc");
}

#endif /* threads/io.h */
