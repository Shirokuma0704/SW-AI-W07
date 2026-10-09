/* 이 파일은 Nachos 교육용 운영체제의 소스 코드에서
   파생됐어요.  Nachos 저작권 고지는 아래에 전문을
   그대로 실었어요. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* 세마포어 SEMA를 VALUE로 초기화해요.  세마포어는
   음이 아닌 정수 하나와, 그 값을 다루는 원자적 연산자
   두 개로 이뤄져요:

   - down 또는 "P": 값이 양수가 될 때까지 기다린 다음
   값을 1 줄여요.

   - up 또는 "V": 값을 1 늘려요(그리고 기다리는
   스레드가 있으면 하나를 깨워요). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* 세마포어의 down 또는 "P" 연산이에요.  SEMA의 값이
   양수가 될 때까지 기다린 다음 원자적으로 1 줄여요.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서
   호출하면 안 돼요.  인터럽트를 끈 상태로 호출할 수는
   있지만, 잠들게 되면 다음에 스케줄되는
   스레드가 아마 인터럽트를 다시 켤 거예요. 이것이
   sema_down 함수예요. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_push_back (&sema->waiters, &thread_current ()->elem);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* 세마포어의 down 또는 "P" 연산이지만, 세마포어가 아직
   0이 아닐 때만 해요.  세마포어 값을 줄였으면 true를,
   아니면 false를 반환해요.

   이 함수는 인터럽트 핸들러에서 호출해도 돼요. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* 세마포어의 up 또는 "V" 연산이에요.  SEMA의 값을 1 늘리고,
   SEMA를 기다리는 스레드가 있으면 그중 하나를 깨워요.

   이 함수는 인터럽트 핸들러에서 호출해도 돼요. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	sema->value++;
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* 한 쌍의 스레드 사이에서 제어가 "ping-pong"하게 만드는
   세마포어 자체 테스트예요.  무슨 일이 일어나는지 보려면
   printf() 호출을 넣어 보세요. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* sema_self_test()가 쓰는 스레드 함수. */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* LOCK을 초기화해요.  락은 한 시점에 최대 한 스레드만
   쥘 수 있어요.  우리 락은 "재귀적"이지 않아요. 즉, 이미
   락을 쥐고 있는 스레드가 같은 락을 다시 획득하려 하면
   오류예요.

   락은 초깃값이 1인 세마포어를 특수화한 거예요.  락과 그런
   세마포어의 차이는 두 가지예요.  첫째, 세마포어는 값이
   1보다 클 수 있지만, 락은 한 시점에 한 스레드만
   소유할 수 있어요.  둘째, 세마포어에는 소유자가 없어서,
   한 스레드가 세마포어를 "down"하고 다른 스레드가
   "up"할 수 있어요.  하지만 락은 같은 스레드가
   획득과 해제를 모두 해야 해요.  이런 제약이
   부담스럽다면,
   락 대신 세마포어를 써야 한다는
   좋은 신호예요. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* LOCK을 획득해요.  필요하면 사용할 수 있게 될 때까지
   잠들어요.  이 락을 현재 스레드가 이미 쥐고 있으면
   안 돼요.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서
   호출하면 안 돼요.  인터럽트를 끈 상태로 호출할 수는
   있지만, 잠들어야 하면 인터럽트가 다시
   켜져요. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
}

/* LOCK 획득을 시도해서 성공하면 true, 실패하면 false를
   반환해요.  이 락을 현재 스레드가 이미 쥐고 있으면
   안 돼요.

   이 함수는 잠들지 않으므로 인터럽트 핸들러 안에서
   호출해도 돼요. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* 현재 스레드가 소유한 LOCK을 해제해요.
   이것이 lock_release 함수예요.

   인터럽트 핸들러는 락을 획득할 수 없으므로, 인터럽트
   핸들러 안에서 락을 해제하려는 것은 말이 되지
   않아요. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* 현재 스레드가 LOCK을 쥐고 있으면 true, 아니면 false를
   반환해요.  (다른 스레드가 락을 쥐고 있는지 검사하는 건
   경합 상태가 될 수 있다는 점에 주의하세요.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* 리스트에 들어 있는 세마포어 하나. */
struct semaphore_elem {
	struct list_elem elem;              /* 리스트 원소. */
	struct semaphore semaphore;         /* 이 세마포어. */
};

/* 조건 변수 COND를 초기화해요.  조건 변수는 어떤 코드가
   조건을 신호로 알리면, 협력하는 다른 코드가 그 신호를 받아서
   그에 따라 동작할 수 있게 해 줘요. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* 원자적으로 LOCK을 해제하고, 다른 코드가 COND에 신호를
   보낼 때까지 기다려요.  COND에 신호가 오면, 반환하기 전에
   LOCK을 다시 획득해요.  이 함수를 호출하기 전에 LOCK을
   쥐고 있어야 해요.

   이 함수가 구현하는 모니터는 "Hoare" 방식이 아니라
   "Mesa" 방식이에요. 즉, 신호를 보내는 것과 받는 것이
   하나의 원자적 연산이 아니에요.  그래서 보통 호출한 쪽은
   기다림이 끝난 뒤 조건을 다시 확인하고, 필요하면 다시
   기다려야 해요.

   하나의 조건 변수는 락 하나와만 연결되지만,
   하나의 락은 원하는 만큼 많은 조건 변수와
   연결될 수 있어요.  즉, 락에서 조건 변수로
   가는 일대다 대응이 있어요.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서
   호출하면 안 돼요.  인터럽트를 끈 상태로 호출할 수는
   있지만, 잠들어야 하면 인터럽트가 다시
   켜져요. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* (LOCK으로 보호되는) COND를 기다리는 스레드가 있으면,
   그중 하나에게 신호를 보내 대기에서 깨어나게 해요.
   이 함수를 호출하기 전에 LOCK을 쥐고 있어야 해요.

   인터럽트 핸들러는 락을 획득할 수 없으므로, 인터럽트
   핸들러 안에서 조건 변수에 신호를 보내려는 것은 말이
   되지 않아요. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* (LOCK으로 보호되는) COND를 기다리는 스레드가 있으면
   모두 깨워요.  이 함수를 호출하기 전에 LOCK을 쥐고 있어야 해요.

   인터럽트 핸들러는 락을 획득할 수 없으므로, 인터럽트
   핸들러 안에서 조건 변수에 신호를 보내려는 것은 말이
   되지 않아요. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
