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
 *      user data driver module
 ***************************************************************************/

#define  LOG_TAG "UserdataDriver"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>

#include "SubtitleLog.h"

#include "UserdataDriver.h"
#include "DemuxDriver.h"
#include "AmlogicTime.h"
#include "AmlogicUtil.h"


//#define EMU_USERDATA
#ifdef EMU_USERDATA
extern const UserdataDriverType emu_ud_drv;
#else
extern const UserdataDriverType aml_ud_drv;
#endif

static UserdataDeviceType userdata_devices[USERDATA_DEV_COUNT] =
{
#ifdef EMU_USERDATA
{
.drv = &emu_ud_drv,
},
#else
{
.drv = &aml_ud_drv,
},
#endif
};

/** Get the device structure pointer based on the device number*/
static inline int userdata_get_dev(int dev_no, UserdataDeviceType **dev) {
    if ((dev_no<0) || (dev_no >= USERDATA_DEV_COUNT)) {
        SUBTITLE_LOGE("invalid userdata device number %d, must in(%d~%d)", dev_no, 0, USERDATA_DEV_COUNT-1);
        return USERDATA_ERROR_INVALID_DEV_NO;
    }
    *dev = &userdata_devices[dev_no];
    return AM_SUCCESS;
}

/** Obtain the device structure according to the device number and check whether the device has been opened*/
static inline int userdata_get_opened_dev(int dev_no, UserdataDeviceType **dev) {
    AM_TRY(userdata_get_dev(dev_no, dev));
    if (!(*dev)->open_cnt) {
        SUBTITLE_LOGE("userdata device %d has not been opened", dev_no);
        return USERDATA_ERROR_INVALID_DEV_NO;
    }
    return AM_SUCCESS;
}

static void dump_user_data(const uint8_t *buff, int size) {
    int i;
    char buf[4096];
    if (size > 1024)
        size = 1024;
    for (i=0; i<size; i++) {
        sprintf(buf+i*3, "%02x ", buff[i]);
    }

    SUBTITLE_LOGI("%s", buf);
}

int userdata_ring_buf_init(UserdataRingBufferType *ringbuf, size_t len) {
    ringbuf->pread=ringbuf->pwrite=0;
    ringbuf->data=(uint8_t*)malloc(len);
    if (ringbuf->data == NULL) return -1;
    ringbuf->size=len;
    ringbuf->error=0;
    pthread_cond_init(&ringbuf->cond, NULL);
    return 0;
}

int userdata_ring_buf_deinit(UserdataRingBufferType *ringbuf){
    ringbuf->pread=ringbuf->pwrite=0;
    if (ringbuf->data != NULL) free(ringbuf->data);
    ringbuf->size=0;
    ringbuf->error=0;
    pthread_cond_destroy(&ringbuf->cond);
    return 0;
}

int userdata_ring_buf_empty(UserdataRingBufferType *ringbuf) {
    return (ringbuf->pread==ringbuf->pwrite);
}

ssize_t userdata_ring_buf_free(UserdataRingBufferType *ringbuf) {
    ssize_t free;
    free = ringbuf->pread - ringbuf->pwrite;
    if (free <= 0) free += ringbuf->size;
    return free-1;
}

ssize_t userdata_ring_buf_avail(UserdataRingBufferType *ringbuf) {
    ssize_t avail;
    avail = ringbuf->pwrite - ringbuf->pread;
    if (avail < 0) avail += ringbuf->size;
    return avail;
}

void userdata_ring_buf_read(UserdataRingBufferType *ringbuf, uint8_t *buf, size_t len) {
    size_t todo = len;
    size_t split;
    if (ringbuf->data == NULL) return;
    split = ((ssize_t)(ringbuf->pread + len) > ringbuf->size) ? ringbuf->size - ringbuf->pread : 0;
    if (split > 0) {
        memcpy(buf, ringbuf->data+ringbuf->pread, split);
        buf += split;
        todo -= split;
        ringbuf->pread = 0;
    }
    memcpy(buf, ringbuf->data+ringbuf->pread, todo);
    ringbuf->pread = (ringbuf->pread + todo) % ringbuf->size;
}

ssize_t userdata_ring_buf_write(UserdataRingBufferType *ringbuf, const uint8_t *buf, size_t len) {
    size_t todo = len;
    size_t split;
    if (ringbuf->data == NULL) return 0;
    split = ((ssize_t)(ringbuf->pwrite + len) > ringbuf->size) ? ringbuf->size - ringbuf->pwrite : 0;
    if (split > 0) {
        memcpy(ringbuf->data+ringbuf->pwrite, buf, split);
        buf += split;
        todo -= split;
        ringbuf->pwrite = 0;
    }
    memcpy(ringbuf->data+ringbuf->pwrite, buf, todo);
    ringbuf->pwrite = (ringbuf->pwrite + todo) % ringbuf->size;
    if (len > 0) pthread_cond_signal(&ringbuf->cond);
    return len;
}

static void read_unused_data(UserdataRingBufferType *ringbuf, size_t len) {
    uint8_t *buf = (uint8_t*)malloc(len);

    if (buf != NULL) {
        //SUBTITLE_LOGE("read %d bytes unused data", len);
        userdata_ring_buf_read(ringbuf, buf, len);
        free(buf);
    }
}

static int userdata_package_write(UserdataDeviceType *dev, const uint8_t *buf, size_t size) {
    int cnt, ret;
    pthread_mutex_lock(&dev->lock);
    cnt = userdata_ring_buf_free(&dev->pkg_buf);
    if (cnt < (int)(size+sizeof(cnt))) {
        SUBTITLE_LOGE("write userdata error: data size to large, %d > %d", size+sizeof(cnt), cnt);
        ret = 0;
    } else {
        cnt = size;
        userdata_ring_buf_write(&dev->pkg_buf, (uint8_t*)&cnt, sizeof(cnt));
        userdata_ring_buf_write(&dev->pkg_buf, buf, size);
        ret = size;
    }
    pthread_mutex_unlock(&dev->lock);

    //SUBTITLE_LOGI("write %d bytes\n", ret);
    dump_user_data(buf, size);
    return ret;
}

static int userdata_package_read(UserdataDeviceType *dev, uint8_t *buf, int size) {
    int cnt, ud_cnt;

    ud_cnt = 0;
    cnt = userdata_ring_buf_avail(&dev->pkg_buf);
    if (cnt > 4) {
        userdata_ring_buf_read(&dev->pkg_buf, (uint8_t*)&ud_cnt, sizeof(ud_cnt));
        cnt = userdata_ring_buf_avail(&dev->pkg_buf);
        if (cnt < ud_cnt) {
            /* this case must not happen */
            //SUBTITLE_LOGI("read userdata error: expect %d bytes, but only %d bytes avail", ud_cnt, cnt);
            cnt = 0;
            read_unused_data(&dev->pkg_buf, cnt);
        } else {
            cnt = 0;
            if (ud_cnt > size) {
                //SUBTITLE_LOGI("read userdata error: source buffer not enough, bufsize %d , datasize %d", size, ud_cnt);
                read_unused_data(&dev->pkg_buf, ud_cnt);
            } else if (ud_cnt > 0) {
                userdata_ring_buf_read(&dev->pkg_buf, buf, ud_cnt);
                cnt = ud_cnt;
            }
        }
    } else {
        //SUBTITLE_LOGI("read userdata error: count = %d < 4", cnt);
        cnt = 0;
        read_unused_data(&dev->pkg_buf, cnt);
    }
    return cnt;
}

static int userdata_package_poll(UserdataDeviceType *dev, int timeout) {
    int rv, ret = AM_SUCCESS, cnt;
    struct timespec rt;

    // not opened or deinitilized.
    // If client call userdata close before stop decode, may has problem.
    // Add more protection here.
    if (dev->open_cnt <= 0) {
        return AM_FAILURE;
    }

    cnt = userdata_ring_buf_avail(&dev->pkg_buf);
    if (cnt <= 0) {
        AmlogicGetTimeSpecTimeout(timeout, &rt);
        rv = pthread_cond_timedwait(&dev->pkg_buf.cond, &dev->lock, &rt);
        if (rv == ETIMEDOUT) {
            //SUBTITLE_LOGE("poll userdata timeout, timeout = %d ms", timeout);
            ret = AM_FAILURE;
        } else {
            cnt = userdata_ring_buf_avail(&dev->pkg_buf);
            if (cnt <= 0) {
                SUBTITLE_LOGE("poll error, unexpected error");
                ret = AM_FAILURE;
            }
        }
    }
    return ret;
}


/** Open the USERDATA device
  * dev_no USERDATA device number
  * USERDATA device opening parameter
  * - AM_SUCCESS Success
  * - other values Error code (see am_userdata.h)
  */
int UserdataOpen(int dev_no, const UserdataOpenParaType *para) {
    UserdataDeviceType *dev;
    int ret = AM_SUCCESS;
    int i;
    assert(para);
    AM_TRY(userdata_get_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gAdpLock);

    if (dev->open_cnt) {
        SUBTITLE_LOGE("userdata device %d has already been opened", dev_no);
        dev->open_cnt++;
        SUBTITLE_LOGE("userdata device AM_USERDATA_Open dev->open_cnt = %d", dev->open_cnt);
        goto final;
    }
    dev->dev_no = dev_no;
    dev->write_package = userdata_package_write;
    userdata_ring_buf_init(&dev->pkg_buf, USERDATA_BUF_SIZE);
    pthread_mutex_init(&dev->lock, NULL);

    if (dev->drv->open) {
        ret = dev->drv->open(dev, para);
    }

    if (ret == AM_SUCCESS) {
        dev->open_cnt++;
    } else {
        pthread_mutex_destroy(&dev->lock);
    }

final:
    pthread_mutex_unlock(&am_gAdpLock);
    return ret;
}

/** Close USERDATA device
  * dev_no USERDATA device number
  * - AM_SUCCESS Success
  * - other values Error code (see am_userdata.h)
  */
int UserdataClose(int dev_no) {
    UserdataDeviceType *dev;
    int ret = AM_SUCCESS;
    AM_TRY(userdata_get_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gAdpLock);

    while (dev->open_cnt > 0) {
        dev->open_cnt--;
        SUBTITLE_LOGE("AM_USERDATA_Close dev->open_cnt = %d",dev->open_cnt);
        if (dev->open_cnt == 0)
        {
            //Notify cond(if has), we're exiting, should break polling
            pthread_cond_broadcast(&dev->pkg_buf.cond);
            if (dev->drv->close) {
                dev->drv->close(dev);
            }
            SUBTITLE_LOGE("UserdataClose pthread_mutex_destroy");
            pthread_mutex_destroy(&dev->lock);
            userdata_ring_buf_deinit(&dev->pkg_buf);
        }
    }
    pthread_mutex_unlock(&am_gAdpLock);
    return ret;
}

/** Read data from USERDATA
  * dev_no USERDATA device number
  * [out] buf buffer
  * size The length of data to be read
  * timeout read timeout ms
  * - actual number of bytes read
  */
int UserdataRead(int dev_no, uint8_t *buf, int size, int timeout_ms) {
    UserdataDeviceType *dev;
    int ret;
    int cnt = -1;
    AM_TRY(userdata_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&dev->lock);
    ret = userdata_package_poll(dev, timeout_ms);
    if (ret == AM_SUCCESS) {
        cnt = userdata_package_read(dev, buf, size);
    }
    pthread_mutex_unlock(&dev->lock);
    return cnt;
}

int UserdataSetMode(int dev_no, int mode) {
    UserdataDeviceType *dev;
    int ret = AM_SUCCESS;
    AM_TRY(userdata_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&dev->lock);
    if (dev->drv->set_mode) ret = dev->drv->set_mode(dev, mode);
    pthread_mutex_unlock(&dev->lock);
    return ret;
}

int UserdataGetMode(int dev_no, int *mode) {
    UserdataDeviceType *dev;
    int ret = AM_SUCCESS;
    if (!mode) return USERDATA_ERROR_INVALID_ARG;
    AM_TRY(userdata_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&dev->lock);
    if (dev->drv->get_mode) ret = dev->drv->get_mode(dev, mode);
    pthread_mutex_unlock(&dev->lock);
    return ret;
}

int UserdataSetParameters(int dev_no, int para) {
    UserdataDeviceType *dev;
    int ret = AM_SUCCESS;
    AM_TRY(userdata_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&dev->lock);
    if (dev->drv->set_param) ret = dev->drv->set_param(dev, para);
    pthread_mutex_unlock(&dev->lock);
    return ret;
}

