#ifndef __SUBTITLE_API_COMMON_H__
#define __SUBTITLE_API_COMMON_H__

typedef enum {
    /*invalid calling, wrong parameter */
    SUB_STAT_INV = -1,
    /* calling failed */
    SUB_STAT_FAIL = 0,
    SUB_STAT_OK,

} AmlSubtitleStatus;
typedef AmlSubtitleStatus SubSourceStatus;

#endif