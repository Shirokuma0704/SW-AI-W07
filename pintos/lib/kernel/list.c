#include "list.h"
#include "../debug.h"

/* 이 이중 연결 리스트에는 헤더 원소가 둘 있어요. 첫 원소 바로 앞의
   "head"와 마지막 원소 바로 뒤의 "tail"이에요. 앞쪽 헤더의
   `prev' 링크는 null이고, 뒤쪽 헤더의 `next' 링크도 null이에요.
   두 헤더의 나머지 링크는 리스트의 내부 원소들을 거쳐
   서로를 향해 가리켜요.

   빈 리스트는 이렇게 생겼어요:

   +------+     +------+
   <---| head |<--->| tail |--->
   +------+     +------+

   원소가 둘인 리스트는 이렇게 생겼어요:

   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
   +------+     +-------+     +-------+     +------+

   이렇게 대칭으로 배치하면 리스트를 처리할 때 생기는 특수한
   경우가 많이 사라져요. 예를 들어 list_remove()를 한번
   보세요. 포인터 대입 두 번이면 되고 조건문은 하나도
   필요 없어요. 헤더 원소가 없을 때의 코드보다 훨씬
   간단해요.

   (각 헤더 원소에서는 포인터 하나만 쓰이므로,
   사실 이 간단함을 잃지 않고도 둘을 헤더 원소 하나로
   합칠 수 있어요. 하지만 원소를 둘로 나눠 두면
   일부 연산에서 약간의 검사를 할 수 있고,
   이것이 쓸모가 있을 수 있어요.) */

static bool is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) UNUSED;

/* ELEM이 head이면 true, 아니면 false를 돌려줘요. */
static inline bool
is_head (struct list_elem *elem) {
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* ELEM이 내부 원소이면 true,
   아니면 false를 돌려줘요. */
static inline bool
is_interior (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* ELEM이 tail이면 true, 아니면 false를 돌려줘요. */
static inline bool
is_tail (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* LIST를 빈 리스트로 초기화해요. */
void
list_init (struct list *list) {
	ASSERT (list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* LIST의 beginning을 돌려줘요. */
struct list_elem *
list_begin (struct list *list) {
	ASSERT (list != NULL);
	return list->head.next;
}

/* 리스트에서 ELEM 다음에 오는 원소를 돌려줘요. ELEM이 그 리스트의
   마지막 원소이면 리스트 tail을 돌려줘요. ELEM 자체가 리스트 tail이면
   결과는 정의되지 않아요. */
struct list_elem *
list_next (struct list_elem *elem) {
	ASSERT (is_head (elem) || is_interior (elem));
	return elem->next;
}

/* LIST의 tail을 돌려줘요.

   list_end()는 리스트를 front에서 back으로 순회할 때 자주
   써요. 예시는 list.h 맨 위의 큰 주석을
   참고하세요. */
struct list_elem *
list_end (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* LIST의 reverse beginning을 돌려줘요. LIST를 back에서 front로
   거꾸로 순회할 때 써요. */
struct list_elem *
list_rbegin (struct list *list) {
	ASSERT (list != NULL);
	return list->tail.prev;
}

/* 리스트에서 ELEM 앞에 있는 원소를 돌려줘요. ELEM이 그 리스트의
   첫 원소이면 리스트 head를 돌려줘요. ELEM 자체가 리스트 head이면
   결과는 정의되지 않아요. */
struct list_elem *
list_prev (struct list_elem *elem) {
	ASSERT (is_interior (elem) || is_tail (elem));
	return elem->prev;
}

/* LIST의 head를 돌려줘요.

   list_rend()는 리스트를 거꾸로, 즉 back에서 front로 순회할 때
   자주 써요. 다음은 list.h 맨 위의 예시에 이어지는
   전형적인 사용법이에요:

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list);
   e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...do something with f...
   }
   */
struct list_elem *
list_rend (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* LIST의 head를 돌려줘요.

   list_head()는 리스트를 순회하는 다른 방식에도 쓸 수 있어요.
   예를 들면:

   e = list_head (&list);
   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   */
struct list_elem *
list_head (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* LIST의 tail을 돌려줘요. */
struct list_elem *
list_tail (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* ELEM을 BEFORE 바로 앞에 끼워 넣어요. BEFORE는 내부 원소이거나
   tail일 수 있어요. tail인 경우는 list_push_back()과
   같아요. */
void
list_insert (struct list_elem *before, struct list_elem *elem) {
	ASSERT (is_interior (before) || is_tail (before));
	ASSERT (elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* 원소 FIRST부터 LAST 직전까지(LAST 제외)를 현재 속한 리스트에서
   떼어 낸 다음, BEFORE 바로 앞에 끼워 넣어요. BEFORE는 내부 원소이거나
   tail일 수 있어요. */
void
list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last) {
	ASSERT (is_interior (before) || is_tail (before));
	if (first == last)
		return;
	last = list_prev (last);

	ASSERT (is_interior (first));
	ASSERT (is_interior (last));

	/* FIRST...LAST를 현재 리스트에서 깔끔하게 떼어 내요. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* FIRST...LAST를 새 리스트에 이어 붙여요. */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* ELEM을 LIST의 beginning에 끼워 넣어서,
   LIST의 front가 되게 해요. */
void
list_push_front (struct list *list, struct list_elem *elem) {
	list_insert (list_begin (list), elem);
}

/* ELEM을 LIST의 끝에 끼워 넣어서,
   LIST의 back이 되게 해요. */
void
list_push_back (struct list *list, struct list_elem *elem) {
	list_insert (list_end (list), elem);
}

/* ELEM을 자신이 속한 리스트에서 제거하고, 그 바로 뒤에 있던
   원소를 돌려줘요. ELEM이 리스트에 없으면 동작이 정의되지 않아요.

   제거한 뒤에는 ELEM을 리스트의 원소로 다루면 안전하지 않아요.
   특히 제거 후의 ELEM에 list_next()나 list_prev()를 쓰면
   동작이 정의되지 않아요. 즉, 리스트의 원소를 제거하는
   단순한 루프는 실패해요:

 ** 이렇게 하지 마세요 **
 for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 {
 ...do something with e...
 list_remove (e);
 }
 ** 이렇게 하지 마세요 **

 리스트를 순회하면서 원소를 제거하는 올바른 방법 하나는
다음과 같아요:

for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

리스트의 원소를 free()해야 한다면 더 조심해야 해요.
이 경우에도 통하는 다른 방법은
다음과 같아요:

while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
*/
struct list_elem *
list_remove (struct list_elem *elem) {
	ASSERT (is_interior (elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* LIST의 front 원소를 제거하고 그 원소를 돌려줘요.
   제거하기 전에 LIST가 비어 있으면 동작이 정의되지 않아요. */
struct list_elem *
list_pop_front (struct list *list) {
	struct list_elem *front = list_front (list);
	list_remove (front);
	return front;
}

/* LIST의 back 원소를 제거하고 그 원소를 돌려줘요.
   제거하기 전에 LIST가 비어 있으면 동작이 정의되지 않아요. */
struct list_elem *
list_pop_back (struct list *list) {
	struct list_elem *back = list_back (list);
	list_remove (back);
	return back;
}

/* LIST의 front 원소를 돌려줘요.
   LIST가 비어 있으면 동작이 정의되지 않아요. */
struct list_elem *
list_front (struct list *list) {
	ASSERT (!list_empty (list));
	return list->head.next;
}

/* LIST의 back 원소를 돌려줘요.
   LIST가 비어 있으면 동작이 정의되지 않아요. */
struct list_elem *
list_back (struct list *list) {
	ASSERT (!list_empty (list));
	return list->tail.prev;
}

/* LIST의 원소 개수를 돌려줘요.
   원소 개수 n에 대해 O(n)으로 실행돼요. */
size_t
list_size (struct list *list) {
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		cnt++;
	return cnt;
}

/* LIST가 비어 있으면 true, 아니면 false를 돌려줘요. */
bool
list_empty (struct list *list) {
	return list_begin (list) == list_end (list);
}

/* A와 B가 가리키는 `struct list_elem *'들을 서로 바꿔요. */
static void
swap (struct list_elem **a, struct list_elem **b) {
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* LIST의 순서를 뒤집어요. */
void
list_reverse (struct list *list) {
	if (!list_empty (list)) {
		struct list_elem *e;

		for (e = list_begin (list); e != list_end (list); e = e->prev)
			swap (&e->prev, &e->next);
		swap (&list->head.next, &list->tail.prev);
		swap (&list->head.next->prev, &list->tail.prev->next);
	}
}

/* 리스트 원소 A부터 B 직전까지(B 제외)가, 보조 데이터 AUX가 주어진
   LESS 기준으로 순서대로 놓여 있을 때만 true를 돌려줘요. */
static bool
is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	if (a != b)
		while ((a = list_next (a)) != b)
			if (less (a, list_prev (a), aux))
				return false;
	return true;
}

/* A에서 시작해 B를 넘지 않는 곳에서 끝나는 리스트 원소들의 구간(run)
   가운데, 보조 데이터 AUX가 주어진 LESS 기준으로 비내림차순인
   구간을 찾아요. 그 구간의 (끝 원소를 포함하지 않는) 끝을
   돌려줘요.
   A부터 B 직전까지는 비어 있지 않은 범위여야 해요. */
static struct list_elem *
find_end_of_run (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	ASSERT (a != NULL);
	ASSERT (b != NULL);
	ASSERT (less != NULL);
	ASSERT (a != b);

	do {
		a = list_next (a);
	} while (a != b && !less (a, list_prev (a), aux));
	return a;
}

/* A0부터 A1B0 직전까지(A1B0 제외)와 A1B0부터 B1 직전까지(B1 제외)를
   병합해서, 마찬가지로 B1(제외)에서 끝나는 하나의 범위를 만들어요.
   두 입력 범위는 모두 비어 있지 않아야 하고, 보조 데이터 AUX가 주어진
   LESS 기준으로 비내림차순으로 정렬되어 있어야 해요.
   출력 범위도 같은 방식으로 정렬돼요. */
static void
inplace_merge (struct list_elem *a0, struct list_elem *a1b0,
		struct list_elem *b1,
		list_less_func *less, void *aux) {
	ASSERT (a0 != NULL);
	ASSERT (a1b0 != NULL);
	ASSERT (b1 != NULL);
	ASSERT (less != NULL);
	ASSERT (is_sorted (a0, a1b0, less, aux));
	ASSERT (is_sorted (a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less (a1b0, a0, aux))
			a0 = list_next (a0);
		else {
			a1b0 = list_next (a1b0);
			list_splice (a0, list_prev (a1b0), a1b0);
		}
}

/* 보조 데이터 AUX가 주어진 LESS 기준으로 LIST를 정렬해요. LIST의
   원소 개수에 대해 O(n lg n) 시간, O(1) 공간으로 실행되는
   자연 반복(natural iterative) 병합 정렬을 써요. */
void
list_sort (struct list *list, list_less_func *less, void *aux) {
	size_t output_run_cnt;        /* 현재 패스에서 출력한 구간(run)의 개수. */

	ASSERT (list != NULL);
	ASSERT (less != NULL);

	/* 리스트를 반복해서 훑으며, 비내림차순 원소들로 이루어진
	   이웃한 구간들을 병합하고, 구간이 하나만 남을 때까지 계속해요. */
	do {
		struct list_elem *a0;     /* 첫 번째 구간의 시작. */
		struct list_elem *a1b0;   /* 첫 번째 구간의 끝, 두 번째 구간의 시작. */
		struct list_elem *b1;     /* 두 번째 구간의 끝. */

		output_run_cnt = 0;
		for (a0 = list_begin (list); a0 != list_end (list); a0 = b1) {
			/* 반복마다 출력 구간 하나를 만들어요. */
			output_run_cnt++;

			/* 비내림차순 원소들로 이루어진 이웃한 두 구간
			   A0...A1B0과 A1B0...B1을 찾아요. */
			a1b0 = find_end_of_run (a0, list_end (list), less, aux);
			if (a1b0 == list_end (list))
				break;
			b1 = find_end_of_run (a1b0, list_end (list), less, aux);

			/* 구간들을 병합해요. */
			inplace_merge (a0, a1b0, b1, less, aux);
		}
	}
	while (output_run_cnt > 1);

	ASSERT (is_sorted (list_begin (list), list_end (list), less, aux));
}

/* LIST에서 ELEM을 알맞은 위치에 끼워 넣어요. LIST는 보조 데이터 AUX가
   주어진 LESS 기준으로 정렬되어 있어야 해요.
   LIST의 원소 개수에 대해 평균적으로 O(n)에 실행돼요. */
void
list_insert_ordered (struct list *list, struct list_elem *elem,
		list_less_func *less, void *aux) {
	struct list_elem *e;

	ASSERT (list != NULL);
	ASSERT (elem != NULL);
	ASSERT (less != NULL);

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		if (less (elem, e, aux))
			break;
	return list_insert (e, elem);
}

/* LIST를 훑으면서, 보조 데이터 AUX가 주어진 LESS 기준으로 서로 같은
   이웃한 원소들의 묶음마다 첫 번째 것만 남기고 나머지를 제거해요.
   DUPLICATES가 null이 아니면, LIST에서 제거한 원소들을
   DUPLICATES에 덧붙여요. */
void
list_unique (struct list *list, struct list *duplicates,
		list_less_func *less, void *aux) {
	struct list_elem *elem, *next;

	ASSERT (list != NULL);
	ASSERT (less != NULL);
	if (list_empty (list))
		return;

	elem = list_begin (list);
	while ((next = list_next (elem)) != list_end (list))
		if (!less (elem, next, aux) && !less (next, elem, aux)) {
			list_remove (next);
			if (duplicates != NULL)
				list_push_back (duplicates, next);
		} else
			elem = next;
}

/* LESS와 보조 데이터 AUX 기준으로 LIST에서 가장 큰 값을 가진 원소를
   돌려줘요. 최댓값이 둘 이상이면 리스트에서 더 앞에 나오는 것을
   돌려줘요. 리스트가 비어 있으면
   리스트의 tail을 돌려줘요. */
struct list_elem *
list_max (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *max = list_begin (list);
	if (max != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (max); e != list_end (list); e = list_next (e))
			if (less (max, e, aux))
				max = e;
	}
	return max;
}

/* LESS와 보조 데이터 AUX 기준으로 LIST에서 가장 작은 값을 가진 원소를
   돌려줘요. 최솟값이 둘 이상이면 리스트에서 더 앞에 나오는 것을
   돌려줘요. 리스트가 비어 있으면
   리스트의 tail을 돌려줘요. */
struct list_elem *
list_min (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *min = list_begin (list);
	if (min != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (min); e != list_end (list); e = list_next (e))
			if (less (e, min, aux))
				min = e;
	}
	return min;
}
