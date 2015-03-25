/*
 * Copyright (c) 2015 Moritz Bitsch <moritzbitsch@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>

#include "coioimpl.h"
#include "coro.h"

static coro_context _sched_ctx;
static unsigned long _taskcount = 0;

CoioTaskList 	coio_ready = {0, 0};
CoioTask       *coio_current;

int
coio_main()
{
	/* initialize empty ctx for scheduler */
	coro_create(&_sched_ctx, NULL, NULL, NULL, 0);

	/* scheduler mainloop */
	for (;;) {
		if (!coio_ready.head)
			break;

		coio_current = coio_ready.head;
		coio_del(&coio_ready, coio_current);
		coro_transfer(&_sched_ctx, &coio_current->ctx);

		if (coio_current->done) {
			_taskcount--;
			coro_stack_free(&coio_current->stk);
			free(coio_current);
		}
		coio_current = NULL;
	}

	if (_taskcount) {
		return -1;
	}
	return 0;
}

static void
_coio_entry(void *arg)
{
	CoioTask       *task = (CoioTask *) arg;

	task->func(task->arg);

	task->done = 1;
	coro_transfer(&coio_current->ctx, &_sched_ctx);
}

int
coio_create(coio_func f, void *arg, unsigned int stacksize)
{
	CoioTask       *task;

	task = calloc(1, sizeof(*task));
	if (!task)
		return -1;

	if (!coro_stack_alloc(&task->stk, stacksize / sizeof(void *))) {
		free(task);
		return -1;
	}
	task->func = f;
	task->arg = arg;

	coro_create(&task->ctx, _coio_entry, task, task->stk.sptr, task->stk.ssze);

	coio_add(&coio_ready, task);
	_taskcount++;

	return 0;
}

void
coio_yield()
{
	coio_add(&coio_ready, coio_current);
	coro_transfer(&coio_current->ctx, &_sched_ctx);
}

void
coio_add(CoioTaskList * lst, CoioTask * task)
{
	if (lst->tail) {
		lst->tail->next = task;
		task->prev = lst->tail;
	} else {
		lst->head = task;
		task->prev = NULL;
	}
	lst->tail = task;
	task->next = NULL;
}

void
coio_del(CoioTaskList * lst, CoioTask * task)
{
	if (task->prev) {
		task->prev->next = task->next;
	} else {
		lst->head = task->next;
	}

	if (task->next) {
		task->next->prev = task->prev;
	} else {
		lst->tail = task->prev;
	}
}
