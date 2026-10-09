#ifndef THREADS_PTE_H
#define THREADS_PTE_H

#include "threads/vaddr.h"

/* x86 하드웨어 페이지 테이블을 다루는 함수와 매크로예요.
 * 가상 주소를 다루는 더 일반적인 함수와 매크로는 vaddr.h를 보세요.
 *
 * 가상 주소는 다음과 같이 구성돼요.
 *  63          48 47            39 38            30 29            21 20         12 11         0
 * +-------------+----------------+----------------+----------------+-------------+------------+
 * | Sign Extend |    Page-Map    | Page-Directory | Page-directory |  Page-Table |  Physical  |
 * |             | Level-4 Offset |    Pointer     |     Offset     |   Offset    |   Offset   |
 * +-------------+----------------+----------------+----------------+-------------+------------+
 *               |                |                |                |             |            |
 *               +------- 9 ------+------- 9 ------+------- 9 ------+----- 9 -----+---- 12 ----+
 *                                            가상 주소
 */

#define PML4SHIFT 39UL
#define PDPESHIFT 30UL
#define PDXSHIFT  21UL
#define PTXSHIFT  12UL

#define PML4(la)  ((((uint64_t) (la)) >> PML4SHIFT) & 0x1FF)
#define PDPE(la) ((((uint64_t) (la)) >> PDPESHIFT) & 0x1FF)
#define PDX(la)  ((((uint64_t) (la)) >> PDXSHIFT) & 0x1FF)
#define PTX(la)  ((((uint64_t) (la)) >> PTXSHIFT) & 0x1FF)
#define PTE_ADDR(pte) ((uint64_t) (pte) & ~0xFFF)

/* 중요한 플래그는 아래와 같아요.
   PDE나 PTE가 "present"가 아니면 나머지 플래그는
   무시돼요.
   0으로 초기화된 PDE나 PTE는 "not present"로
   해석되는데, 그래도 아무 문제 없어요. */
#define PTE_FLAGS 0x00000000000000fffUL    /* 플래그 비트. */
#define PTE_ADDR_MASK  0xffffffffffffff000UL /* 주소 비트. */
#define PTE_AVL   0x00000e00             /* OS가 쓸 수 있는 비트. */
#define PTE_P 0x1                        /* 1=present, 0=not present. */
#define PTE_W 0x2                        /* 1=읽기/쓰기, 0=읽기 전용. */
#define PTE_U 0x4                        /* 1=사용자/커널, 0=커널 전용. */
#define PTE_A 0x20                       /* 1=접근됨, 0=접근 안 됨. */
#define PTE_D 0x40                       /* 1=더티, 0=더티 아님 (PTE에서만). */

#endif /* threads/pte.h */
