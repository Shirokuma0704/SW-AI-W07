/* 스레드 N개를 만들고, 각 스레드는 서로 다른 고정된 시간만큼
   M번 잠들어요. 깨어난 순서를 기록하고 그 순서가 올바른지
   확인해요. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void test_sleep (int thread_cnt, int iterations);

void
test_alarm_single (void) 
{
  test_sleep (5, 1);
}

void
test_alarm_multiple (void) 
{
  test_sleep (5, 7);
}

/* 테스트 정보. */
struct sleep_test 
  {
    int64_t start;              /* 테스트를 시작하는 시각. */
    int iterations;             /* 스레드마다 반복할 횟수. */

    /* 출력. */
    struct lock output_lock;    /* 출력 버퍼를 보호하는 락. */
    int *output_pos;            /* 출력 버퍼에서 지금 쓸 위치. */
  };

/* 테스트 안의 스레드 하나에 대한 정보. */
struct sleep_thread 
  {
    struct sleep_test *test;     /* 모든 스레드가 함께 쓰는 정보. */
    int id;                     /* 잠드는 스레드의 ID. */
    int duration;               /* 한 번에 잠들 tick 수. */
    int iterations;             /* 지금까지 센 반복 횟수. */
  };

static void sleeper (void *);

/* 스레드 THREAD_CNT개가 각각 ITERATIONS번씩 잠들게 해요. */
static void
test_sleep (int thread_cnt, int iterations) 
{
  struct sleep_test test;
  struct sleep_thread *threads;
  int *output, *op;
  int product;
  int i;

  /* 이 테스트는 MLFQS에서는 동작하지 않아요. */
  ASSERT (!thread_mlfqs);

  msg ("Creating %d threads to sleep %d times each.", thread_cnt, iterations);
  msg ("Thread 0 sleeps 10 ticks each time,");
  msg ("thread 1 sleeps 20 ticks each time, and so on.");
  msg ("If successful, product of iteration count and");
  msg ("sleep duration will appear in nondescending order.");

  /* 메모리 할당. */
  threads = malloc (sizeof *threads * thread_cnt);
  output = malloc (sizeof *output * iterations * thread_cnt * 2);
  if (threads == NULL || output == NULL)
    PANIC ("couldn't allocate memory for test");

  /* 테스트 초기화. */
  test.start = timer_ticks () + 100;
  test.iterations = iterations;
  lock_init (&test.output_lock);
  test.output_pos = output;

  /* 스레드 시작. */
  ASSERT (output != NULL);
  for (i = 0; i < thread_cnt; i++)
    {
      struct sleep_thread *t = threads + i;
      char name[16];
      
      t->test = &test;
      t->id = i;
      t->duration = (i + 1) * 10;
      t->iterations = 0;

      snprintf (name, sizeof name, "thread %d", i);
      thread_create (name, PRI_DEFAULT, sleeper, t);
    }
  
  /* 모든 스레드가 끝날 만큼 충분히 기다려요. */
  timer_sleep (100 + thread_cnt * iterations * 10 + 100);

  /* 혹시 아직 돌고 있는 말썽꾼 스레드가 있을 수 있으니
     출력 락을 잡아요. */
  lock_acquire (&test.output_lock);

  /* 깨어난 순서를 출력해요. */
  product = 0;
  for (op = output; op < test.output_pos; op++) 
    {
      struct sleep_thread *t;
      int new_prod;

      ASSERT (*op >= 0 && *op < thread_cnt);
      t = threads + *op;

      new_prod = ++t->iterations * t->duration;
        
      msg ("thread %d: duration=%d, iteration=%d, product=%d",
           t->id, t->duration, t->iterations, new_prod);
      
      if (new_prod >= product)
        product = new_prod;
      else
        fail ("thread %d woke up out of order (%d > %d)!",
              t->id, product, new_prod);
    }

  /* 깨어난 횟수가 맞는지 확인해요. */
  for (i = 0; i < thread_cnt; i++)
    if (threads[i].iterations != iterations)
      fail ("thread %d woke up %d times instead of %d",
            i, threads[i].iterations, iterations);
  
  lock_release (&test.output_lock);
  free (output);
  free (threads);
}

/* 잠드는 스레드. */
static void
sleeper (void *t_) 
{
  struct sleep_thread *t = t_;
  struct sleep_test *test = t->test;
  int i;

  for (i = 1; i <= test->iterations; i++) 
    {
      int64_t sleep_until = test->start + i * t->duration;
      timer_sleep (sleep_until - timer_ticks ());
      lock_acquire (&test->output_lock);
      *test->output_pos++ = t->id;
      lock_release (&test->output_lock);
    }
}
