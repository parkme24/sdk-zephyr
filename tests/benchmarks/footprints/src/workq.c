/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <zephyr.h>
#include <syscall_handler.h>

#include "footprint.h"

static struct k_work_q workq;
static K_THREAD_STACK_DEFINE(workq_stack, STACK_SIZE);

struct k_sem sync_sema;

#ifdef CONFIG_USERSPACE
static struct k_work_q user_workq;
static K_THREAD_STACK_DEFINE(user_workq_stack, STACK_SIZE);

static FP_BMEM struct k_work user_work_item;
#endif

void workq_func(struct k_work *unused)
{
	ARG_UNUSED(unused);

	k_sem_give(&sync_sema);
}

void simple_workq_thread(void *arg1, void *arg2, void *arg3)
{
	struct k_work work_item;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	k_sem_reset(&sync_sema);
	k_work_init(&work_item, workq_func);
	k_work_submit_to_queue(&workq, &work_item);

	k_sem_take(&sync_sema, K_FOREVER);
}

void delayed_workq_thread(void *arg1, void *arg2, void *arg3)
{
	struct k_delayed_work work_item;

	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	k_sem_reset(&sync_sema);
	k_delayed_work_init(&work_item, workq_func);
	k_delayed_work_submit_to_queue(&workq, &work_item, K_NO_WAIT);

	k_sem_take(&sync_sema, K_FOREVER);
}

#ifdef CONFIG_USERSPACE
void simple_user_workq_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	k_sem_reset(&sync_sema);
	k_work_init(&user_work_item, workq_func);
	k_work_submit_to_user_queue(&user_workq, &user_work_item);

	k_sem_take(&sync_sema, K_FOREVER);
}
#endif

void run_workq(void)
{
	k_tid_t tid;

	k_sem_init(&sync_sema, 0, 1);

	k_work_q_start(&workq, workq_stack,
		       K_THREAD_STACK_SIZEOF(workq_stack),
		       CONFIG_MAIN_THREAD_PRIORITY);

	/* Exercise simple workqueue */
	tid = k_thread_create(&my_thread, my_stack_area, STACK_SIZE,
			      simple_workq_thread, NULL, NULL, NULL,
			      0, 0, K_NO_WAIT);

	k_thread_join(tid, K_FOREVER);

	/* Exercise delayed workqueue */
	tid = k_thread_create(&my_thread, my_stack_area, STACK_SIZE,
			      delayed_workq_thread, NULL, NULL, NULL,
			      0, 0, K_NO_WAIT);

	k_thread_join(tid, K_FOREVER);

#ifdef CONFIG_USERSPACE
	k_work_q_user_start(&user_workq, user_workq_stack,
			    K_THREAD_STACK_SIZEOF(user_workq_stack),
			    CONFIG_MAIN_THREAD_PRIORITY);

	k_mem_domain_add_thread(&footprint_mem_domain, &user_workq.thread);
	k_thread_access_grant(&user_workq.thread, &user_workq_stack);
	k_thread_access_grant(&user_workq.thread, &sync_sema);

	tid = k_thread_create(&my_thread, my_stack_area, STACK_SIZE,
			      simple_user_workq_thread, NULL, NULL, NULL,
			      0, K_USER, K_NO_WAIT);

	k_thread_access_grant(tid, &sync_sema,
			      &user_workq.thread, &user_workq.queue,
			      &user_workq_stack);

	k_mem_domain_add_thread(&footprint_mem_domain, tid);

	k_thread_start(tid);
	k_thread_join(tid, K_FOREVER);

#endif
}
