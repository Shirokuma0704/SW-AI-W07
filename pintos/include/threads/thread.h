#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* 스레드 생애 주기의 상태들. */
enum thread_status {
	THREAD_RUNNING,     /* 실행 중인 스레드. */
	THREAD_READY,       /* 실행 중은 아니지만 실행할 준비가 된 상태. */
	THREAD_BLOCKED,     /* 어떤 이벤트가 일어나기를 기다리는 상태. */
	THREAD_DYING        /* 곧 파괴될 상태. */
};

/* 스레드 식별자 타입.
   원하는 타입으로 다시 정의해도 돼요. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* tid_t의 오류 값. */

/* 스레드 우선순위. */
#define PRI_MIN 0                       /* 가장 낮은 우선순위. */
#define PRI_DEFAULT 31                  /* 기본 우선순위. */
#define PRI_MAX 63                      /* 가장 높은 우선순위. */

/* 커널 스레드 또는 사용자 프로세스.
 *
 * 각 스레드 구조체는 자기만의 4 kB 페이지에 저장돼요.  스레드
 * 구조체 자체는 페이지의 맨 아래(오프셋 0)에 있어요.  페이지의
 * 나머지는 스레드의 커널 스택을 위해 남겨 두는데, 이 스택은
 * 페이지의 맨 위(오프셋 4 kB)에서 아래쪽으로 자라요.  그림으로
 * 보면 이래요:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * 이렇게 되면 두 가지 결론이 나와요:
 *
 *    1. 첫째, `struct thread'가 너무 커지면 안 돼요.
 *       커지면 커널 스택을 둘 공간이 모자라게 돼요.
 *       기본 `struct thread'는 크기가 몇 바이트밖에
 *       안 돼요.  아마 1 kB보다 훨씬 작게
 *       유지하는 게 좋을 거예요.
 *
 *    2. 둘째, 커널 스택이 너무 커지면 안 돼요.  스택이
 *       넘치면 스레드 상태가 망가져요.  그래서 커널 함수는
 *       큰 구조체나 배열을 static이 아닌 지역 변수로
 *       할당하면 안 돼요.  대신 malloc()이나
 *       palloc_get_page()로 동적 할당을
 *       쓰세요.
 *
 * 이 두 문제 중 어느 쪽이든 첫 증상은 아마
 * thread_current()의 assertion 실패일 거예요.  이 함수는
 * 실행 중인 스레드의 `struct thread' 중 `magic' 멤버가
 * THREAD_MAGIC으로 설정돼 있는지 확인해요.  스택 오버플로는
 * 보통 이 값을 바꿔서 assertion을 일으켜요. */
/* `elem' 멤버는 두 가지 용도가 있어요.  실행 큐(thread.c)의
 * 원소가 될 수도 있고, 세마포어 대기 리스트(synch.c)의
 * 원소가 될 수도 있어요.  이 두 가지로 모두 쓸 수 있는 건
 * 둘이 서로 배타적이기 때문이에요. ready 상태의 스레드만
 * 실행 큐에 있고, blocked 상태의 스레드만 세마포어 대기
 * 리스트에 있어요. */
struct thread {
	/* thread.c가 소유해요. */
	tid_t tid;                          /* 스레드 식별자. */
	enum thread_status status;          /* 스레드 상태. */
	char name[16];                      /* 이름(디버깅용). */
	int priority;                       /* 우선순위. */

	/* thread.c와 synch.c가 공유해요. */
	struct list_elem elem;              /* 리스트 원소. */

#ifdef USERPROG
	/* userprog/process.c가 소유해요. */
	uint64_t *pml4;                     /* 페이지 맵 레벨 4 */
#endif
#ifdef VM
	/* 스레드가 소유한 전체 가상 메모리용 테이블. */
	struct supplemental_page_table spt;
#endif

	/* thread.c가 소유해요. */
	struct intr_frame tf;               /* 전환에 쓰는 정보 */
	unsigned magic;                     /* 스택 오버플로를 감지해요. */
};

/* false(기본값)이면 round-robin 스케줄러를 써요.
   true면 multi-level feedback queue 스케줄러를 써요.
   커널 커맨드라인 옵션 "-o mlfqs"로 제어해요. */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

#endif /* threads/thread.h */
