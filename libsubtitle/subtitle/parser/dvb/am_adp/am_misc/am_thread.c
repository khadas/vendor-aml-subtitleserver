/***************************************************************************
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description:
 */
/**\file
 * \brief pthread debugging tools
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-10-15: create the document
 ***************************************************************************/
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define  LOG_TAG "AM_THREAD"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "SubtitleLog.h"

#include <am_mem.h>

/****************************************************************************
 * Type define
 ***************************************************************************/

typedef struct {
    const char       *file;
    const char       *func;
    int               line;
} AM_ThreadFrame_t;

typedef struct AM_Thread AM_Thread_t;
struct AM_Thread {
    AM_Thread_t      *prev;
    AM_Thread_t      *next;
    pthread_t         thread;
    char             *name;
    void* (*entry)(void*);
    void             *arg;
    AM_ThreadFrame_t *frame;
    int               frame_size;
    int               frame_top;
};

/****************************************************************************
 * Static data
 ***************************************************************************/
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t  once = PTHREAD_ONCE_INIT;
static AM_Thread_t    *threads = NULL;

/****************************************************************************
 * Static functions
 ***************************************************************************/

static void thread_init(void)
{
    AM_Thread_t *th;

    th = malloc(sizeof(AM_Thread_t));
    if (!th)
    {
        SUBTITLE_LOGE("not enough memory");
        return;
    }

    th->thread= pthread_self();
    th->name  = strdup("main");
    th->entry = NULL;
    th->arg   = NULL;
    th->prev  = NULL;
    th->next  = NULL;
    th->frame = NULL;
    th->frame_size = 0;
    th->frame_top  = 0;

    threads = th;

    SUBTITLE_LOGE("Register thread \"main\"");
}

static void thread_remove(AM_Thread_t *th)
{
    if (th->prev)
        th->prev->next = th->next;
    else
        threads = th->next;
    if (th->next)
        th->next->prev = th->prev;

    if (th->name)
        free(th->name);
    if (th->frame)
        free(th->frame);

    free(th);
}

static void* thread_entry(void *arg)
{
    AM_Thread_t *th = (AM_Thread_t*)arg;
    void *r;

    pthread_mutex_lock(&lock);
    th->thread = pthread_self();
    pthread_mutex_unlock(&lock);

    SUBTITLE_LOGE("Register thread \"%s\" %p", th->name, (void*)th->thread);

    r = th->entry(th->arg);

    pthread_mutex_lock(&lock);
    thread_remove(th);
    pthread_mutex_unlock(&lock);

    return r;
}

static AM_Thread_t* thread_get(pthread_t t)
{
    AM_Thread_t *th;

    for (th=threads; th; th=th->next)
    {
        if (th->thread == t)
            return th;
    }

    return NULL;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 创建一个被AM_THREAD管理的线程
 * \param[out] thread 返回线程句柄
 * \param[in] attr 线程属性，等于NULL时使用缺省属性
 * \param start_routine 线程入口函数
 * \param[in] arg 线程入口函数的参数
 * \param[in] name 线程名
 * \return 成功返回0，失败返回错误代码
 */
int AM_pthread_create_name(pthread_t *thread,
        const pthread_attr_t *attr,
        void* (*start_routine)(void*),
        void *arg,
        const char *name)
{
    AM_Thread_t *th;
    int ret;

    pthread_once(&once, thread_init);

    th = malloc(sizeof(AM_Thread_t));
    if (!th)
    {
        SUBTITLE_LOGE("not enough memory");
        return -1;
    }

    th->thread= -1;
    th->name  = name?strdup(name):NULL;
    th->entry = start_routine;
    th->arg   = arg;
    th->prev  = NULL;
    th->frame = NULL;
    th->frame_size = 0;
    th->frame_top  = 0;

    pthread_mutex_lock(&lock);
    if (threads)
        threads->prev = th;
    th->next = threads;
    threads = th;
    pthread_mutex_unlock(&lock);

    ret = pthread_create(thread, attr, thread_entry, th);
    if (ret)
    {
        SUBTITLE_LOGE("create thread failed");
        pthread_mutex_lock(&lock);
        thread_remove(th);
        pthread_mutex_unlock(&lock);

        return ret;
    }

    return 0;
}

/**\brief 结束当前线程
 * \param[in] r 返回值
 */
void AM_pthread_exit(void *r)
{
    AM_Thread_t *th;

    pthread_once(&once, thread_init);

    pthread_mutex_lock(&lock);

    th = thread_get(pthread_self());
    if (th)
        thread_remove(th);
    else
        SUBTITLE_LOGE("thread %p is not registered", (void*)pthread_self());

    pthread_mutex_unlock(&lock);

    pthread_exit(r);
}

/**\brief 记录当前线程进入一个函数
 * \param[in] file 文件名
 * \param[in] func 函数名
 * \param line 行号
 * \return 成功返回0，失败返回错误代码
 */
int AM_pthread_enter(const char *file, const char *func, int line)
{
    AM_Thread_t *th;
    int ret = 0;

    pthread_once(&once, thread_init);

    pthread_mutex_lock(&lock);

    th = thread_get(pthread_self());
    if (th)
    {
        if ((th->frame_top+1) >= th->frame_size)
        {
            AM_ThreadFrame_t *f;
            int size = AM_MAX(th->frame_size*2, 16);

            f = realloc(th->frame, sizeof(AM_ThreadFrame_t)*size);
            if (!f)
            {
                SUBTITLE_LOGE("not enough memory");
                ret = -1;
                goto error;
            }

            th->frame = f;
            th->frame_size = size;
        }
        th->frame[th->frame_top].file = file;
        th->frame[th->frame_top].func = func;
        th->frame[th->frame_top].line = line;
        th->frame_top++;
    }
    else
    {
        SUBTITLE_LOGE("thread %p is not registered", (void*)pthread_self());
        ret = -1;
    }
error:
    pthread_mutex_unlock(&lock);
    return ret;
}

/**\brief 记录当前线程离开一个函数
 * \param[in] file 文件名
 * \param[in] func 函数名
 * \param line 行号
 * \return 成功返回0，失败返回错误代码
 */
int AM_pthread_leave(const char *file, const char *func, int line)
{
    AM_Thread_t *th;

    pthread_once(&once, thread_init);

    pthread_mutex_lock(&lock);

    th = thread_get(pthread_self());
    if (th)
    {
        if (!th->frame_top)
            SUBTITLE_LOGE("AM_pthread_enter and AM_pthread_leave mismatch");
        else
            th->frame_top--;
    }
    else
    {
        SUBTITLE_LOGE("thread %p is not registered", (void*)pthread_self());
    }

    pthread_mutex_unlock(&lock);

    return 0;
}

/**\brief 打印当前所有注册线程的状态信息
 * \return 成功返回0，失败返回错误代码
 */
int AM_pthread_dump(void)
{
    AM_Thread_t *th;
    int i, l, n;

    pthread_once(&once, thread_init);

    pthread_mutex_lock(&lock);

    for (th=threads,i=0; th; th=th->next, i++)
    {
        if (th->thread == -1)
            continue;

        fprintf(stdout, "Thread %d (%p:%s)\n", i, (void*)th->thread, th->name?th->name:"");
        for (l=th->frame_top-1,n=0; l>=0; l--,n++)
        {
            AM_ThreadFrame_t *f = &th->frame[l];
            fprintf(stdout, "\t<%d> %s line %d [%s]\n", n, f->func?f->func:NULL, f->line, f->file?f->file:NULL);
        }
    }

    pthread_mutex_unlock(&lock);

    return 0;
}

