#ifndef THREADS_INTR_STUBS_H
#define THREADS_INTR_STUBS_H

/* 인터럽트 stub.
 *
 * intr-stubs.S에 있는 작은 코드 조각들로, 가능한 256개의 x86
 * 인터럽트마다 하나씩 있어요. 각각 스택을 조금 조작한 다음
 * intr_entry()로 점프해요.
 * 더 자세한 내용은 intr-stubs.S를 참고하세요.
 *
 * 이 배열은 각 인터럽트 stub의 진입점을 가리켜서
 * intr_init()이 쉽게 찾을 수 있게 해요. */
typedef void intr_stub_func (void);
extern intr_stub_func *intr_stubs[256];

#endif /* threads/intr-stubs.h */
