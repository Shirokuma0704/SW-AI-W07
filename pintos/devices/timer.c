#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* 8254 타이머 칩의 하드웨어 세부 사항은 [8254]를 보세요. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* OS가 부팅된 이후의 timer tick 수. */
static int64_t ticks;

/* timer tick 하나당 루프 횟수.
   timer_calibrate()가 초기화해요. */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* 8254 Programmable Interval Timer(PIT)가 초당 PIT_FREQ번
   인터럽트를 일으키도록 설정하고, 그에 해당하는 인터럽트를
   등록해요. */
void
timer_init (void) {
	/* 8254 입력 주파수를 TIMER_FREQ로 나누고,
	   가장 가까운 정수로 반올림해요. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: 카운터 0, LSB 다음 MSB, 모드 2, 이진수. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* 짧은 지연을 구현하는 데 쓰는 loops_per_tick을 보정해요. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* loops_per_tick을 timer tick 하나보다 여전히 작은
	   가장 큰 2의 거듭제곱으로 근사해요. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* loops_per_tick의 다음 8비트를 정밀하게 맞춰요. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* OS가 부팅된 이후의 timer tick 수를 반환해요. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* THEN 이후 지난 timer tick 수를 반환해요.  THEN은 한 번
   timer_ticks()가 반환했던 값이어야 해요. */
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* 대략 TICKS timer tick 동안 실행을 멈춰요. */
void
timer_sleep (int64_t ticks) {
	int64_t start = timer_ticks ();

	ASSERT (intr_get_level () == INTR_ON);
	while (timer_elapsed (start) < ticks)
		thread_yield ();
}

/* 대략 MS 밀리초 동안 실행을 멈춰요. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* 대략 US 마이크로초 동안 실행을 멈춰요. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* 대략 NS 나노초 동안 실행을 멈춰요. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* timer 통계를 출력해요. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer 인터럽트 핸들러. */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_tick ();
}

/* LOOPS번 반복하는 데 timer tick 하나보다 오래 걸리면 true,
   아니면 false를 반환해요. */
static bool
too_many_loops (unsigned loops) {
	/* timer tick을 기다려요. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* LOOPS번 반복해요. */
	start = ticks;
	busy_wait (loops);

	/* tick 수가 바뀌었다면 너무 오래 반복한 거예요. */
	barrier ();
	return start != ticks;
}

/* 짧은 지연을 구현하기 위해 단순한 루프를
   LOOPS번 반복해요.

   NO_INLINE으로 표시한 이유는 코드 정렬이 타이밍에
   크게 영향을 줄 수 있기 때문이에요.  이 함수가 곳곳에서
   서로 다르게 인라인되면 결과를
   예측하기 어려워져요. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* 대략 NUM/DENOM초 동안 잠들어요. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* NUM/DENOM초를 timer tick으로 바꾸고, 내림해요.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* 최소 timer tick 하나는 통째로 기다려야 해요.  timer_sleep()은
		   CPU를 다른 프로세스에 양보하므로
		   이 함수를 써요. */
		timer_sleep (ticks);
	} else {
		/* 그렇지 않으면 tick보다 짧은 시간을 더 정확하게 맞추려고
		   busy-wait 루프를 써요.  오버플로 가능성을 피하려고 분자와
		   분모를 1000으로 나눠서 줄여요. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
