#pragma once


#ifdef ANDROID
#include <utils/Log.h>

#define LOGD ALOGD
#define LOGE ALOGE
#define LOGI ALOGI
#define LOGV ALOGV
#define LOGW ALOGW

#define  TRACE()    LOGI("[%s::%d]\n",__FUNCTION__,__LINE__)

#else
/*#define ALOGD(...)
#define ALOGE(...)
#define ALOGI(...)
#define ALOGV(...)
#define ALOGW(...)

#define LOGD(...)
#define LOGE(...)
#define LOGI(...)
#define LOGV(...)
#define TRACE(...)
*/
#endif

