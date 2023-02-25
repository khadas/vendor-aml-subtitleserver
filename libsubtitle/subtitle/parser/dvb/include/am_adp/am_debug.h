/****************************************************************************
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
 *
 * \file
 * \brief DEBUG 信息输出设置
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-12: create the document
 ***************************************************************************/

#ifndef _AM_DEBUG_H
#define _AM_DEBUG_H

#include "am_util.h"
#include <stdio.h>
#include "am_misc.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#ifndef AM_DEBUG_LEVEL
#define AM_DEBUG_LEVEL 500
#endif

#define AM_DEBUG_LOGLEVEL_DEFAULT 1

#define AM_DEBUG_WITH_FILE_LINE
#ifdef AM_DEBUG_WITH_FILE_LINE
#define AM_DEBUG_FILE_LINE fprintf(stderr, "(\"%s\" %d)", __FILE__, __LINE__);
#else
#define AM_DEBUG_FILE_LINE
#endif

/**\brief 输出调试信息
 *如果_level小于等于文件中定义的宏AM_DEBUG_LEVEL,则输出调试信息。
 */
#ifndef ANDROID
#define AM_DEBUG(_level,_fmt...) \
	AM_MACRO_BEGIN\
	if ((_level)<=(AM_DEBUG_LEVEL))\
	{\
		fprintf(stderr, "AM_DEBUG:");\
		AM_DEBUG_FILE_LINE\
		fprintf(stderr, _fmt);\
		fprintf(stderr, "\n");\
	}\
	AM_MACRO_END
#else
#include <android/log.h>
#ifndef TAG_EXT
#define TAG_EXT
#endif

#define log_print(...) __android_log_print(ANDROID_LOG_INFO, "AM_DEBUG" TAG_EXT, __VA_ARGS__)
#define AM_DEBUG(_level,_fmt...) \
	AM_MACRO_BEGIN\
	if ((_level)<=(AM_DebugGetLogLevel()))\
	{\
		log_print(_fmt);\
	}\
	AM_MACRO_END
#endif
/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/****************************************************************************
 * Function prototypes
 ***************************************************************************/

#ifdef __cplusplus
}
#endif

#endif

