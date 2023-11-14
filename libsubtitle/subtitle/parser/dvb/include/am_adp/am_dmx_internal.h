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
 * \brief Demultiplexing device drivers
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-21: create the document
 ***************************************************************************/

#ifndef _AM_DMX_INTERNAL_H
#define _AM_DMX_INTERNAL_H

#include <am_dmx.h>
#include <am_util.h>
#include <am_thread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define DMX_FILTER_COUNT      (32)
#define DMX_WAKE_POLL_EVENT   (1)
#define DMX_FILTER_COUNT_EXT  DMX_FILTER_COUNT + DMX_WAKE_POLL_EVENT


#define DMX_FL_RUN_CB         (1)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 解复用设备*/
typedef struct AM_DMX_Device AM_DMX_Device_t;

/**\brief 过滤器*/
typedef struct AM_DMX_Filter AM_DMX_Filter_t;

/**\brief 过滤器位屏蔽*/
typedef uint32_t AM_DMX_FilterMask_t;

#define AM_DMX_FILTER_MASK_ISEMPTY(m)    (!(*(m)))
#define AM_DMX_FILTER_MASK_CLEAR(m)      (*(m)=0)
#define AM_DMX_FILTER_MASK_ISSET(m,i)    (*(m)&(((unsigned int)1<<(i)) & 0xFFFFFFFF))
#define AM_DMX_FILTER_MASK_SET(m,i)      (*(m)|=((unsigned int)1<<(i)))

/**\brief 解复用设备驱动*/
typedef struct
{
    AM_ErrorCode_t (*open)(AM_DMX_Device_t *dev, const AM_DMX_OpenPara_t *para);
    AM_ErrorCode_t (*close)(AM_DMX_Device_t *dev);
    AM_ErrorCode_t (*wake)(AM_DMX_Device_t *dev);
    AM_ErrorCode_t (*alloc_filter)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter);
    AM_ErrorCode_t (*free_filter)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter);
    AM_ErrorCode_t (*set_sec_filter)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, const struct dmx_sct_filter_params *params);
    AM_ErrorCode_t (*set_pes_filter)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, const struct dmx_pes_filter_params *params);
    AM_ErrorCode_t (*enable_filter)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, AM_Bool_t enable);
    AM_ErrorCode_t (*set_buf_size)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, int size);
    AM_ErrorCode_t (*poll)(AM_DMX_Device_t *dev, AM_DMX_FilterMask_t *mask, int timeout);
    AM_ErrorCode_t (*read)(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, uint8_t *buf, int *size);
    AM_ErrorCode_t (*set_source)(AM_DMX_Device_t *dev, AM_DMX_Source_t src);
} AM_DMX_Driver_t;

/**\brief Section过滤器*/
struct AM_DMX_Filter
{
    void      *drv_data; /**< 驱动私有数据*/
    AM_Bool_t  used;     /**< 此Filter是否已经分配*/
    AM_Bool_t  enable;   /**< 此Filter设备是否使能*/
    int        id;       /**< Filter ID*/
    AM_DMX_DataCb       cb;        /**< 解复用数据回调函数*/
    void               *user_data; /**< 数据回调函数用户参数*/
};

/**\brief 解复用设备*/
struct AM_DMX_Device
{
    int                 dev_no;  /**< 设备号*/
    const AM_DMX_Driver_t *drv;  /**< 设备驱动*/
    void               *drv_data;/**< 驱动私有数据*/
    AM_DMX_Filter_t     filters[DMX_FILTER_COUNT];   /**< 设备中的Filter*/
    int                 open_count; /**< 设备已经打开次数*/
    AM_Bool_t           enable_thread; /**< 数据线程已经运行*/
    int                 flags;   /**< 线程运行状态控制标志*/
    pthread_t           thread;  /**< 数据检测线程*/
    pthread_mutex_t     lock;    /**< 设备保护互斥体*/
    pthread_cond_t      cond;    /**< 条件变量*/
    AM_DMX_Source_t     src;     /**< TS输入源*/
};


/****************************************************************************
 * Function prototypes
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

