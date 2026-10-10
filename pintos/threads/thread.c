#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* struct thread의 `magic' 멤버에 쓰는 임의의 값.
   스택 오버플로를 감지하는 데 써요.  자세한 내용은 thread.h
   맨 위의 큰 주석을 보세요. */
#define THREAD_MAGIC 0xcd6abf4b

/* 기본 스레드를 나타내는 임의의 값
   이 값은 수정하지 마세요. */
#define THREAD_BASIC 0xd42df210

/* THREAD_READY 상태에 있는 프로세스들의 리스트예요. 즉,
   실행할 준비는 됐지만 아직 실행 중은 아닌 프로세스들이에요. */
static struct list ready_list;

/* Idle 스레드. */
static struct thread *idle_thread;

/* 초기 스레드, init.c:main()을 실행하는 스레드. */
static struct thread *initial_thread;

/* allocate_tid()가 쓰는 락. */
static struct lock tid_lock;

/* 스레드 파괴 요청 */
static struct list destruction_req;

/* 통계. */
static long long idle_ticks;    /* idle 상태로 보낸 timer tick 수. */
static long long kernel_ticks;  /* 커널 스레드에서 보낸 timer tick 수. */
static long long user_ticks;    /* 사용자 프로그램에서 보낸 timer tick 수. */

/* 스케줄링. */
#define TIME_SLICE 4            /* 각 스레드에 주는 timer tick 수. */
static unsigned thread_ticks;   /* 마지막 양보 이후의 timer tick 수. */

/* false(기본값)이면 round-robin 스케줄러를 써요.
   true면 multi-level feedback queue 스케줄러를 써요.
   커널 커맨드라인 옵션 "-o mlfqs"로 제어해요. */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);
int MAX(int a, int b) { return a > b ? a : b; }

static bool priority_sort(const struct list_elem *,const struct list_elem *, void *aux UNUSED);

/* T가 올바른 스레드를 가리키는 것처럼 보이면 true를 반환해요. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* 실행 중인 스레드를 반환해요.
 * CPU의 스택 포인터 `rsp'를 읽은 다음, 그 값을 페이지의
 * 시작 주소로 내려요.  `struct thread'는 항상 페이지의 맨 앞에
 * 있고 스택 포인터는 그 중간 어딘가에 있으니, 이렇게 하면
 * 현재 스레드를 찾을 수 있어요. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// thread_start용 전역 디스크립터 테이블(GDT).
// gdt는 thread_init 이후에 설정되기 때문에,
// 먼저 임시 gdt를 설정해야 해요.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* 지금 실행 중인 코드를 스레드로 변환해서 스레드 시스템을
   초기화해요.  일반적으로는 이런 방식이 동작하지 않지만, 여기서는
   loader.S가 스택의 바닥을 페이지 경계에 맞춰 두었기 때문에
   가능해요.

   실행 큐와 tid 락도 함께 초기화해요.

   이 함수를 호출한 뒤에는, thread_create()로 스레드를
   만들기 전에 반드시 페이지 할당자를
   먼저 초기화해야 해요.

   이 함수가 끝나기 전에는 thread_current()를 호출하면
   안전하지 않아요. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* 커널용 임시 gdt를 다시 불러와요.
	 * 이 gdt에는 사용자 컨텍스트가 들어 있지 않아요.
	 * 커널은 gdt_init ()에서 사용자 컨텍스트를 포함해 gdt를 다시 만들어요. */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* 전역 스레드 컨텍스트를 초기화해요 */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);

	/* 실행 중인 스레드용 스레드 구조체를 설정해요. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* 인터럽트를 켜서 선점형 스레드 스케줄링을 시작해요.
   idle 스레드도 함께 만들어요. */
void
thread_start (void) {
	/* Idle 스레드를 만들어요. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* 선점형 스레드 스케줄링을 시작해요. */
	intr_enable ();

	/* idle 스레드가 idle_thread를 초기화할 때까지 기다려요. */
	sema_down (&idle_started);
}

/* timer tick마다 timer 인터럽트 핸들러가 호출해요.
   그래서 이 함수는 외부 인터럽트 컨텍스트에서 실행돼요. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* 통계를 갱신해요. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* 선점을 강제해요. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* 스레드 통계를 출력해요. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* 이름이 NAME이고 초기 우선순위가 PRIORITY인 새 커널 스레드를 만들어요.
   이 스레드는 AUX를 인자로 FUNCTION을 실행하고, ready 큐에
   추가돼요.  새 스레드의 스레드 식별자를 반환하고, 생성에
   실패하면 TID_ERROR를 반환해요.

   thread_start()가 이미 호출됐다면, thread_create()가 반환되기
   전에 새 스레드가 스케줄될 수도 있어요.  심지어 thread_create()가
   반환되기 전에 종료될 수도 있어요.  반대로, 새 스레드가 스케줄되기
   전까지 원래 스레드가 얼마든지 오래 실행될 수도 있어요.  실행
   순서를 보장해야 한다면 세마포어나 다른 동기화 수단을
   쓰세요.

   제공된 코드는 새 스레드의 `priority' 멤버를 PRIORITY로
   설정하지만, 실제 우선순위 스케줄링은 구현돼 있지 않아요.
   우선순위 스케줄링은 Problem 1-3의 목표예요. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* 스레드를 할당해요. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* 스레드를 초기화해요. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* 스케줄되면 kernel_thread를 호출해요.
	 * 참고) rdi는 첫 번째 인자, rsi는 두 번째 인자예요. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* 실행 큐에 추가해요. */
	thread_unblock (t);

	// 스레드를 만든 직후 Read_list
	if (list_empty (&ready_list) != true && priority > running_thread()->priority)
		thread_yield ();

	return tid;
}

/* 현재 스레드를 재워요.  thread_unblock()이 깨워 주기 전까지는
   다시 스케줄되지 않아요.

   이 함수는 인터럽트를 끈 상태로 호출해야 해요.  보통은
   synch.h에 있는 동기화 기본 요소 중 하나를 쓰는 편이
   더 좋아요. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* 블록된 스레드 T를 실행 가능(ready) 상태로 전환해요.
   T가 블록 상태가 아니면 오류예요.  (실행 중인 스레드를 ready로
   만들려면 thread_yield()를 쓰세요.)

   이 함수는 실행 중인 스레드를 선점하지 않아요.  이게 중요할
   수 있는데, 호출한 쪽이 직접 인터럽트를 꺼 둔 경우에는 스레드를
   unblock하면서 다른 데이터를 갱신하는 일을 원자적으로
   할 수 있다고 기대할 수 있기 때문이에요. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_insert_ordered (&ready_list, &t->elem, &priority_sort, NULL);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* 실행 중인 스레드의 이름을 반환해요. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* 실행 중인 스레드를 반환해요.
   running_thread()에 몇 가지 안전성 검사를 더한 거예요.
   자세한 내용은 thread.h 맨 위의 큰 주석을 보세요. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* T가 정말 스레드인지 확인해요.
	   이 assertion 둘 중 하나라도 터지면, 스레드가 스택을
	   넘쳐 썼을 수 있어요.  각 스레드의 스택은 4 kB도 안 되기
	   때문에, 큰 자동 배열 몇 개나 적당한 깊이의 재귀만으로도
	   스택 오버플로가 날 수 있어요. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* 실행 중인 스레드의 tid를 반환해요. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* 현재 스레드를 스케줄에서 내리고 파괴해요.  호출한 쪽으로
   절대 돌아가지 않아요. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* 상태만 dying으로 바꾸고 다른 프로세스를 스케줄해요.
	   파괴는 schedule_tail()을 호출하는 중에 일어나요. */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* CPU를 양보해요.  현재 스레드는 잠들지 않고, 스케줄러의 판단에 따라
   곧바로 다시 스케줄될 수도 있어요. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (curr != idle_thread)
		list_insert_ordered (&ready_list, &curr->elem, &priority_sort, NULL);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* 현재 스레드의 우선순위를 NEW_PRIORITY로 설정해요. */
void
thread_set_priority (int new_priority) {

	struct thread *curr = thread_current ();
	if (list_empty(&curr->donate_list) == true)
		curr->priority = new_priority;
	else
		curr->priority = MAX(list_entry( list_front(&curr->donate_list), struct thread, donate_elem)->priority, new_priority);
	
	thread_current ()->initial_priority = new_priority;
// 레디 리스트가 비어있지 않으며, 기존 스래드의 새 우선도가 다음 최우선 스레드의 우선도보다 낮을때
	// 기존 스래드가 Ready_list에 정렬삽입되며 Running thread변경
	if (list_empty (&ready_list) != true && curr->priority < list_entry(list_front(&ready_list), struct thread, elem)->priority)
	{
		thread_yield();
	}
}

/* 현재 스레드의 우선순위를 반환해요. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* 현재 스레드의 nice 값을 NICE로 설정해요. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: 여기에 구현을 작성하세요 */
}

/* 현재 스레드의 nice 값을 반환해요. */
int
thread_get_nice (void) {
	/* TODO: 여기에 구현을 작성하세요 */
	return 0;
}

/* 시스템 load average의 100배를 반환해요. */
int
thread_get_load_avg (void) {
	/* TODO: 여기에 구현을 작성하세요 */
	return 0;
}

/* 현재 스레드의 recent_cpu 값의 100배를 반환해요. */
int
thread_get_recent_cpu (void) {
	/* TODO: 여기에 구현을 작성하세요 */
	return 0;
}

/* Idle 스레드.  실행할 준비가 된 다른 스레드가 없을 때 실행돼요.

   idle 스레드는 처음에 thread_start()가 ready 리스트에
   넣어요.  처음에 한 번 스케줄되면 idle_thread를 초기화하고,
   thread_start()가 계속 진행할 수 있도록 넘겨받은 세마포어를
   "up"한 다음 곧바로 블록돼요.  그 뒤로 idle 스레드는 ready
   리스트에 나타나지 않아요.  ready 리스트가 비어 있을 때는
   특수한 경우로 next_thread_to_run()이 idle 스레드를
   반환해요. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* 다른 스레드가 실행하게 해요. */
		intr_disable ();
		thread_block ();

		/* 인터럽트를 다시 켜고 다음 인터럽트를 기다려요.

		   `sti' 명령은 다음 명령이 끝날 때까지
		   인터럽트를 막기 때문에, 이 두 명령은
		   원자적으로 실행돼요.  이 원자성이 중요해요.
		   그렇지 않으면 인터럽트를 다시 켜는 시점과
		   다음 인터럽트를 기다리는 시점 사이에
		   인터럽트가 처리돼서, 클럭 tick 하나만큼의
		   시간을 낭비할 수 있어요.

		   [IA32-v2a] "HLT", [IA32-v2b] "STI", [IA32-v3a]
		   7.11.1 "HLT Instruction"을 보세요. */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* 커널 스레드의 기반으로 쓰는 함수. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* 스케줄러는 인터럽트를 끈 채로 실행돼요. */
	function (aux);       /* 스레드 함수를 실행해요. */
	thread_exit ();       /* function()이 반환하면 스레드를 종료해요. */
}


/* T를 이름이 NAME인 블록된 스레드로
   기본 초기화해요. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->initial_priority = priority;
	t->magic = THREAD_MAGIC;
	list_init (&t->donate_list);
}

/* 다음에 스케줄할 스레드를 골라서 반환해요.  실행 큐가
   비어 있지 않다면 실행 큐에서 스레드를 반환해야 해요.
   (실행 중인 스레드가 계속 실행될 수 있다면 그 스레드도
   실행 큐에 들어 있을 거예요.)  실행 큐가 비어 있으면
   idle_thread를 반환해요. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* iretq를 써서 스레드를 시작해요 */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* 새 스레드의 페이지 테이블을 활성화해서 스레드를 전환하고,
   이전 스레드가 dying 상태라면 그 스레드를 파괴해요.

   이 함수가 호출된 시점에는 방금 스레드 PREV에서 전환됐고,
   새 스레드가 이미 실행 중이며, 인터럽트는 아직
   꺼져 있어요.

   스레드 전환이 끝나기 전에는 printf()를 호출하면 안전하지
   않아요.  실제로는 printf()를 함수의 맨 끝에
   추가해야 한다는 뜻이에요. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* 핵심 전환 로직이에요.
	 * 먼저 전체 실행 컨텍스트를 intr_frame에 복원한 다음,
	 * do_iret을 호출해서 다음 스레드로 전환해요.
	 * 전환이 끝날 때까지는 여기서부터 어떤 스택도
	 * 쓰면 안 돼요. */
	__asm __volatile (
			/* 사용할 레지스터들을 저장해요. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* 입력을 한 번만 가져와요 */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // 저장해 둔 rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // 저장해 둔 rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // 저장해 둔 rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // 현재 rip를 읽어요.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* 새 프로세스를 스케줄해요. 진입 시점에 인터럽트는 꺼져 있어야 해요.
 * 이 함수는 현재 스레드의 상태를 status로 바꾼 다음,
 * 실행할 다른 스레드를 찾아서 그 스레드로 전환해요.
 * schedule() 안에서 printf()를 호출하는 건 안전하지 않아요. */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* 실행 중으로 표시해요. */
	next->status = THREAD_RUNNING;

	/* 새 타임 슬라이스를 시작해요. */
	thread_ticks = 0;

#ifdef USERPROG
	/* 새 주소 공간을 활성화해요. */
	process_activate (next);
#endif

	if (curr != next) {
		/* 전환해 나오는 스레드가 dying 상태라면, 그 struct
		   thread를 파괴해요.  thread_exit()가 자기 발밑의 바닥을
		   빼 버리는 일이 없도록 이 작업은 늦게 일어나야 해요.
		   페이지가 지금 스택으로 쓰이고 있기 때문에, 여기서는
		   페이지 해제 요청을 큐에 넣기만 해요.
		   실제 파괴 로직은 schedule()의 시작 부분에서
		   호출돼요. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* 스레드를 전환하기 전에, 먼저 현재 실행 중인
		 * 스레드의 정보를 저장해요. */
		thread_launch (next);
	}
}

/* 새 스레드가 쓸 tid를 반환해요. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}


static bool priority_sort(const struct list_elem *a,const struct list_elem *b, void *aux UNUSED)
{
	int64_t a_priority = list_entry(a, struct thread, elem)->priority;
	int64_t b_priority = list_entry(b, struct thread, elem)->priority;

	if (a_priority > b_priority) return true;
	return false;
}
