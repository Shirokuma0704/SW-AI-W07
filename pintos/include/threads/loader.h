#ifndef THREADS_LOADER_H
#define THREADS_LOADER_H

/* PC BIOS가 정해 둔 상수들. */
#define LOADER_BASE 0x7c00      /* 로더 시작 지점의 물리 주소. */
#define LOADER_END  0x7e00      /* 로더 끝 지점의 물리 주소. */

/* 커널 시작 지점의 물리 주소. */
#define LOADER_KERN_BASE 0x8004000000

/* 모든 물리 메모리가 매핑되는 커널 가상 주소. */
#define LOADER_PHYS_BASE 0x200000

/* Multiboot 정보 */
#define MULTIBOOT_INFO       0x7000
#define MULTIBOOT_FLAG       MULTIBOOT_INFO
#define MULTIBOOT_MMAP_LEN   MULTIBOOT_INFO + 44
#define MULTIBOOT_MMAP_ADDR  MULTIBOOT_INFO + 48

#define E820_MAP MULTIBOOT_INFO + 52
#define E820_MAP4 MULTIBOOT_INFO + 56

/* 중요한 로더 물리 주소들. */
#define LOADER_SIG (LOADER_END - LOADER_SIG_LEN)   /* 0xaa55 BIOS 시그니처. */
#define LOADER_ARGS (LOADER_SIG - LOADER_ARGS_LEN)     /* 명령줄 인자. */
#define LOADER_ARG_CNT (LOADER_ARGS - LOADER_ARG_CNT_LEN) /* 인자 개수. */

/* 로더 자료구조의 크기. */
#define LOADER_SIG_LEN 2
#define LOADER_ARGS_LEN 128
#define LOADER_ARG_CNT_LEN 4

/* 로더가 정의하는 GDT 셀렉터.
   더 많은 셀렉터는 userprog/gdt.h에 정의돼 있어요. */
#define SEL_NULL        0x00    /* 널 셀렉터. */
#define SEL_KCSEG       0x08    /* 커널 코드 셀렉터. */
#define SEL_KDSEG       0x10    /* 커널 데이터 셀렉터. */
#define SEL_UDSEG       0x1B    /* 사용자 데이터 셀렉터. */
#define SEL_UCSEG       0x23    /* 사용자 코드 셀렉터. */
#define SEL_TSS         0x28    /* 태스크 상태 세그먼트. */
#define SEL_CNT         8       /* 세그먼트 수. */

#endif /* threads/loader.h */
