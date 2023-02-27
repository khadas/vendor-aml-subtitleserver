#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
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
 * \brief Linux DVB demux 驱动
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-07-21: create the document
 ***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
//#include <am_mem.h>
#include <am_misc.h>
#include "am_dmx_internal.h"
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
/*add for config define for linux dvb *.h*/
#include <am_config.h>
#include <linux/dvb/dmx.h>
#include <sys/eventfd.h>
#include <stdlib.h>


#define open(a...)\
	({\
	 int ret, times=3;\
	 do {\
        ret = open(a);\
        if (ret == -1)\
        {\
           usleep(100*1000);\
        }\
	 }while(ret==-1 && times--);\
	 ret;\
	 })

/****************************************************************************
 * Type definitions
 ***************************************************************************/

typedef struct
{
	char   dev_name[32];
	int    fd[DMX_FILTER_COUNT];
	int    event_fd;
} DVBDmx_t;

/****************************************************************************
 * Static data definitions
 ***************************************************************************/

static AM_ErrorCode_t dvb_open(AM_DMX_Device_t *dev, const AM_DMX_OpenPara_t *para);
static AM_ErrorCode_t dvb_close(AM_DMX_Device_t *dev);
static AM_ErrorCode_t dvb_wake(AM_DMX_Device_t *dev);
static AM_ErrorCode_t dvb_alloc_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter);
static AM_ErrorCode_t dvb_free_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter);
static AM_ErrorCode_t dvb_set_sec_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, const struct dmx_sct_filter_params *params);
static AM_ErrorCode_t dvb_set_pes_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, const struct dmx_pes_filter_params *params);
static AM_ErrorCode_t dvb_enable_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, AM_Bool_t enable);
static AM_ErrorCode_t dvb_set_buf_size(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, int size);
static AM_ErrorCode_t dvb_poll(AM_DMX_Device_t *dev, AM_DMX_FilterMask_t *mask, int timeout);
static AM_ErrorCode_t dvb_read(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, uint8_t *buf, int *size);
static AM_ErrorCode_t dvb_set_source(AM_DMX_Device_t *dev, AM_DMX_Source_t src);

AM_DMX_Driver_t linux_dvb_dmx_drv = {
.open  = dvb_open,
.close = dvb_close,
.alloc_filter = dvb_alloc_filter,
.free_filter  = dvb_free_filter,
.set_sec_filter = dvb_set_sec_filter,
.set_pes_filter = dvb_set_pes_filter,
.enable_filter  = dvb_enable_filter,
.set_buf_size   = dvb_set_buf_size,
.poll           = dvb_poll,
.read           = dvb_read,
.wake           = dvb_wake,
.set_source     = dvb_set_source
};


/****************************************************************************
 * Static functions
 ***************************************************************************/

static AM_ErrorCode_t dvb_open(AM_DMX_Device_t *dev, const AM_DMX_OpenPara_t *para)
{
	DVBDmx_t *dmx;
	int i;

	UNUSED(para);

	dmx = (DVBDmx_t*)malloc(sizeof(DVBDmx_t));
	if (!dmx)
	{
		AM_DEBUG(1, "not enough memory");
		return AM_DMX_ERR_NO_MEM;
	}

	snprintf(dmx->dev_name, sizeof(dmx->dev_name), "/dev/dvb0.demux%d", dev->dev_no);

	for (i=0; i<DMX_FILTER_COUNT; i++)
		dmx->fd[i] = -1;

	dev->drv_data = dmx;
	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_close(AM_DMX_Device_t *dev)
{
	DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
	if (dmx->event_fd >= 0)
	{
	    close(dmx->event_fd);
	    dmx->event_fd = -1;
	}

	free(dmx);
	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_alloc_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter)
{
	DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
	int fd;
	int event_fd;

	fd = open(dmx->dev_name, O_RDWR);
	if (fd == -1)
	{
		AM_DEBUG(1, "cannot open \"%s\" (%s)", dmx->dev_name, strerror(errno));
		return AM_DMX_ERR_CANNOT_OPEN_DEV;
	}

	dmx->fd[filter->id] = fd;

	event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (event_fd >= 0)
	{
	    dmx->event_fd = event_fd;
	}


	filter->drv_data = (void*)(long)fd;

	return AM_SUCCESS;
}


static AM_ErrorCode_t dvb_free_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter)
{
	DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
	int fd = (long)filter->drv_data;

	close(fd);
	dmx->fd[filter->id] = -1;

	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_set_sec_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, const struct dmx_sct_filter_params *params)
{
	struct dmx_sct_filter_params p;
	int fd = (long)filter->drv_data;
	int ret;

	UNUSED(dev);

	p = *params;
	/*
	if (p.filter.mask[0] == 0) {
		p.filter.filter[0] = 0x00;
		p.filter.mask[0]   = 0x80;
	}
	*/
	ret = ioctl(fd, DMX_SET_FILTER, &p);
	if (ret == -1)
	{
		AM_DEBUG(1, "set section filter failed (%s)", strerror(errno));
		return AM_DMX_ERR_SYS;
	}

	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_set_pes_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, const struct dmx_pes_filter_params *params)
{
	int fd = (long)filter->drv_data;
	int ret;

	UNUSED(dev);

	//TODO for coverity check_return
	ret = fcntl(fd,F_SETFL,O_NONBLOCK);
	if (ret == -1)
	{
		AM_DEBUG(1, "fcntl failed (%s)", strerror(errno));
		return AM_DMX_ERR_SYS;
	}

	ret = ioctl(fd, DMX_SET_PES_FILTER, params);
	if (ret == -1)
	{
		AM_DEBUG(1, "set section filter failed (%s)", strerror(errno));
		return AM_DMX_ERR_SYS;
	}

	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_enable_filter(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, AM_Bool_t enable)
{
	int fd = (long)filter->drv_data;
	int ret;

	UNUSED(dev);

	if (enable)
		ret = ioctl(fd, DMX_START, 0);
	else
		ret = ioctl(fd, DMX_STOP, 0);

	if (ret == -1)
	{
		AM_DEBUG(1, "start filter failed (%s)", strerror(errno));
		return AM_DMX_ERR_SYS;
	}

	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_set_buf_size(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, int size)
{
	int fd = (long)filter->drv_data;
	int ret;

	UNUSED(dev);

	ret = ioctl(fd, DMX_SET_BUFFER_SIZE, size);
	if (ret == -1)
	{
		AM_DEBUG(1, "set buffer size failed (%s)", strerror(errno));
		return AM_DMX_ERR_SYS;
	}

	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_poll(AM_DMX_Device_t *dev, AM_DMX_FilterMask_t *mask, int timeout)
{
	DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
	struct pollfd fds[DMX_FILTER_COUNT_EXT];
	int fids[DMX_FILTER_COUNT_EXT] = {0};
	int i, cnt = 0, ret;

	for (i = 0; i < DMX_FILTER_COUNT; i++)
	{
		if (dmx->fd[i] != -1)
		{
			fds[cnt].events = POLLIN|POLLERR;
			fds[cnt].fd     = dmx->fd[i];
			fids[cnt] = i;
			cnt++;
		}
	}

	//add notify event fd
	if (dmx->event_fd != -1)
	{
		fds[cnt].events = POLLIN|POLLERR;
		fds[cnt].fd     = dmx->event_fd;
		cnt++;
	}

	if (!cnt)
		return AM_DMX_ERR_TIMEOUT;

	ret = poll(fds, cnt, timeout);
	if (ret <= 0)
	{
		return AM_DMX_ERR_TIMEOUT;
	}

	for (i = 0; i < cnt; i++)
	{
		if (fds[i].revents&(POLLIN|POLLERR))
		{
			AM_DMX_FILTER_MASK_SET(mask, fids[i]);
		}
	}
	return AM_SUCCESS;
}

/*
 *function:@dvb_poll timeout too long, this function can wake up poll advance.
 */
static AM_ErrorCode_t dvb_wake(AM_DMX_Device_t *dev)
{
	DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
	uint64_t wdata  = 0;

	int event_fd = dmx->event_fd;
	int ret = write(event_fd, &wdata, 8);
	if (ret != 8)
	{
	    AM_DEBUG(0, "dvb_wake write %d bytes instead of 8!", ret);
	}

	return AM_SUCCESS;
}


static AM_ErrorCode_t dvb_read(AM_DMX_Device_t *dev, AM_DMX_Filter_t *filter, uint8_t *buf, int *size)
{
	int fd = (long)filter->drv_data;
	int len = *size;
	int ret;
	struct pollfd pfd;

	UNUSED(dev);

	if (fd == -1)
		return AM_DMX_ERR_NOT_ALLOCATED;

	pfd.events = POLLIN|POLLERR;
	pfd.fd     = fd;

	ret = poll(&pfd, 1, 0);
	if (ret <= 0)
		return AM_DMX_ERR_NO_DATA;

	ret = read(fd, buf, len);
	if (ret <= 0)
	{
		if (errno == ETIMEDOUT)
			return AM_DMX_ERR_TIMEOUT;
		AM_DEBUG(1, "read demux failed (%s) %d", strerror(errno), errno);
		return AM_DMX_ERR_SYS;
	}

	*size = ret;
	return AM_SUCCESS;
}

static AM_ErrorCode_t dvb_set_source(AM_DMX_Device_t *dev, AM_DMX_Source_t src)
{
	char buf[32];
	char *cmd;

	snprintf(buf, sizeof(buf), "/sys/class/stb/demux%d_source", dev->dev_no);

	switch (src)
	{
		case AM_DMX_SRC_TS0:
			cmd = "ts0";
		break;
		case AM_DMX_SRC_TS1:
			cmd = "ts1";
		break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
		case AM_DMX_SRC_TS2:
			cmd = "ts2";
		break;
#endif
		case AM_DMX_SRC_TS3:
			cmd = "ts3";
		break;
		case AM_DMX_SRC_HIU:
			cmd = "hiu";
		break;
		case AM_DMX_SRC_HIU1:
			cmd = "hiu1";
		break;
		default:
			AM_DEBUG(1, "do not support demux source %d", src);
		return AM_DMX_ERR_NOT_SUPPORTED;
	}

	return AM_FileEcho(buf, cmd);
}
#ifdef __cplusplus
}
#endif

