#include "threads/malloc.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* malloc()의 간단한 구현이에요.

   각 요청의 크기(바이트)는 2의 거듭제곱으로 올림되고, 그
   크기의 블록을 관리하는 "디스크립터"에 배정돼요.
   디스크립터는 빈 블록 리스트를 갖고 있어요. 이 free list가
   비어 있지 않으면 그 안의 블록 하나로 요청을
   처리해요.

   비어 있으면 페이지 할당기에서 "아레나"라고 부르는 새
   메모리 페이지를 얻어요(하나도 없으면 malloc()은 널
   포인터를 반환해요). 새 아레나는 블록들로 나뉘고, 그
   블록이 전부 디스크립터의 free list에 추가돼요. 그다음
   새 블록 중 하나를 반환해요.

   블록을 해제하면 그 블록을 디스크립터의 free list에
   추가해요. 그런데 블록이 있던 아레나에 사용 중인 블록이
   더 이상 없으면, 그 아레나의 블록을 전부 free list에서
   제거하고 아레나를 페이지 할당기에 돌려줘요.

   이 방식으로는 2 kB보다 큰 블록을 처리할 수 없어요.
   디스크립터와 함께 한 페이지에 들어가기에는 너무 크기
   때문이에요. 그런 블록은 페이지 할당기로 연속된 페이지를
   할당하고, 할당된 블록의 아레나 헤더 맨 앞에 할당 크기를
   적어 두는 방식으로 처리해요. */

/* 디스크립터. */
struct desc {
	size_t block_size;          /* 각 요소의 크기(바이트). */
	size_t blocks_per_arena;    /* 아레나 하나에 들어가는 블록 수. */
	struct list free_list;      /* 빈 블록의 리스트. */
	struct lock lock;           /* 락. */
};

/* 아레나가 손상됐는지 감지하기 위한 매직 넘버. */
#define ARENA_MAGIC 0x9a548eed

/* 아레나. */
struct arena {
	unsigned magic;             /* 항상 ARENA_MAGIC으로 설정돼요. */
	struct desc *desc;          /* 소유한 디스크립터. 큰 블록이면 널. */
	size_t free_cnt;            /* 빈 블록 수. 큰 블록이면 페이지 수. */
};

/* 빈 블록. */
struct block {
	struct list_elem free_elem; /* free list 요소. */
};

/* 디스크립터 모음. */
static struct desc descs[10];   /* 디스크립터들. */
static size_t desc_cnt;         /* 디스크립터 수. */

static struct arena *block_to_arena (struct block *);
static struct block *arena_to_block (struct arena *, size_t idx);

/* malloc() 디스크립터들을 초기화해요. */
void
malloc_init (void) {
	size_t block_size;

	for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2) {
		struct desc *d = &descs[desc_cnt++];
		ASSERT (desc_cnt <= sizeof descs / sizeof *descs);
		d->block_size = block_size;
		d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / block_size;
		list_init (&d->free_list);
		lock_init (&d->lock);
	}
}

/* 최소 SIZE바이트인 새 블록을 얻어서 반환해요.
   메모리가 없으면 널 포인터를 반환해요. */
void *
malloc (size_t size) {
	struct desc *d;
	struct block *b;
	struct arena *a;

	/* 0바이트 요청은 널 포인터로 충족돼요. */
	if (size == 0)
		return NULL;

	/* SIZE바이트 요청을 만족하는 가장 작은 디스크립터를
	   찾아요. */
	for (d = descs; d < descs + desc_cnt; d++)
		if (d->block_size >= size)
			break;
	if (d == descs + desc_cnt) {
		/* SIZE가 어느 디스크립터에도 맞지 않을 만큼 커요.
		   SIZE와 아레나를 담을 만큼 페이지를 할당해요. */
		size_t page_cnt = DIV_ROUND_UP (size + sizeof *a, PGSIZE);
		a = palloc_get_multiple (0, page_cnt);
		if (a == NULL)
			return NULL;

		/* PAGE_CNT개 페이지짜리 큰 블록임을 나타내도록 아레나를
		   초기화하고 반환해요. */
		a->magic = ARENA_MAGIC;
		a->desc = NULL;
		a->free_cnt = page_cnt;
		return a + 1;
	}

	lock_acquire (&d->lock);

	/* free list가 비어 있으면 새 아레나를 만들어요. */
	if (list_empty (&d->free_list)) {
		size_t i;

		/* 페이지 하나를 할당해요. */
		a = palloc_get_page (0);
		if (a == NULL) {
			lock_release (&d->lock);
			return NULL;
		}

		/* 아레나를 초기화하고 그 블록들을 free list에 추가해요. */
		a->magic = ARENA_MAGIC;
		a->desc = d;
		a->free_cnt = d->blocks_per_arena;
		for (i = 0; i < d->blocks_per_arena; i++) {
			struct block *b = arena_to_block (a, i);
			list_push_back (&d->free_list, &b->free_elem);
		}
	}

	/* free list에서 블록 하나를 꺼내 반환해요. */
	b = list_entry (list_pop_front (&d->free_list), struct block, free_elem);
	a = block_to_arena (b);
	a->free_cnt--;
	lock_release (&d->lock);
	return b;
}

/* 0으로 초기화된 A × B 바이트를 할당해서 반환해요.
   메모리가 없으면 널 포인터를 반환해요. */
void *
calloc (size_t a, size_t b) {
	void *p;
	size_t size;

	/* 블록 크기를 계산하고 size_t에 들어가는지 확인해요. */
	size = a * b;
	if (size < a || size < b)
		return NULL;

	/* 메모리를 할당하고 0으로 채워요. */
	p = malloc (size);
	if (p != NULL)
		memset (p, 0, size);

	return p;
}

/* BLOCK에 할당된 바이트 수를 반환해요. */
static size_t
block_size (void *block) {
	struct block *b = block;
	struct arena *a = block_to_arena (b);
	struct desc *d = a->desc;

	return d != NULL ? d->block_size : PGSIZE * a->free_cnt - pg_ofs (block);
}

/* OLD_BLOCK의 크기를 NEW_SIZE바이트로 바꿔 봐요. 그 과정에서
   옮겨질 수도 있어요.
   성공하면 새 블록을 반환하고, 실패하면 널 포인터를
   반환해요.
   OLD_BLOCK이 널이면 malloc(NEW_SIZE)와 같아요.
   NEW_SIZE가 0이면 free(OLD_BLOCK)과 같아요. */
void *
realloc (void *old_block, size_t new_size) {
	if (new_size == 0) {
		free (old_block);
		return NULL;
	} else {
		void *new_block = malloc (new_size);
		if (old_block != NULL && new_block != NULL) {
			size_t old_size = block_size (old_block);
			size_t min_size = new_size < old_size ? new_size : old_size;
			memcpy (new_block, old_block, min_size);
			free (old_block);
		}
		return new_block;
	}
}

/* 블록 P를 해제해요. P는 앞서 malloc(), calloc(),
   realloc() 중 하나로 할당된 것이어야 해요. */
void
free (void *p) {
	if (p != NULL) {
		struct block *b = p;
		struct arena *a = block_to_arena (b);
		struct desc *d = a->desc;

		if (d != NULL) {
			/* 일반 블록이에요. 여기서 처리해요. */

#ifndef NDEBUG
			/* use-after-free 버그를 찾는 데 도움이 되도록 블록을 지워요. */
			memset (b, 0xcc, d->block_size);
#endif

			lock_acquire (&d->lock);

			/* 블록을 free list에 추가해요. */
			list_push_front (&d->free_list, &b->free_elem);

			/* 아레나가 이제 완전히 안 쓰이면 해제해요. */
			if (++a->free_cnt >= d->blocks_per_arena) {
				size_t i;

				ASSERT (a->free_cnt == d->blocks_per_arena);
				for (i = 0; i < d->blocks_per_arena; i++) {
					struct block *b = arena_to_block (a, i);
					list_remove (&b->free_elem);
				}
				palloc_free_page (a);
			}

			lock_release (&d->lock);
		} else {
			/* 큰 블록이에요. 그 페이지들을 해제해요. */
			palloc_free_multiple (a, a->free_cnt);
			return;
		}
	}
}

/* 블록 B가 들어 있는 아레나를 반환해요. */
static struct arena *
block_to_arena (struct block *b) {
	struct arena *a = pg_round_down (b);

	/* 아레나가 유효한지 확인해요. */
	ASSERT (a != NULL);
	ASSERT (a->magic == ARENA_MAGIC);

	/* 블록이 아레나에 맞게 제대로 정렬돼 있는지 확인해요. */
	ASSERT (a->desc == NULL
			|| (pg_ofs (b) - sizeof *a) % a->desc->block_size == 0);
	ASSERT (a->desc != NULL || pg_ofs (b) == sizeof *a);

	return a;
}

/* 아레나 A 안의 (IDX - 1)번째 블록을 반환해요. */
static struct block *
arena_to_block (struct arena *a, size_t idx) {
	ASSERT (a != NULL);
	ASSERT (a->magic == ARENA_MAGIC);
	ASSERT (idx < a->desc->blocks_per_arena);
	return (struct block *) ((uint8_t *) a
			+ sizeof *a
			+ idx * a->desc->block_size);
}
