/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef AMLOGIC_SUBTITLECMD_H
#define AMLOGIC_SUBTITLECMD_H

typedef enum subtitlecmd_e {
    //PQ CMD(201~299)
    SUBTITLE_MOUDLE_CMD_START           = 400,
    SUBTITLE_OPENCONNECTION           = 401,
    SUBTITLE_CLOSECONNECTION          = 402,

    SUBTITLE_OPEN = 403,
    SUBTITLE_CLOSE=404,
    SUBTITLE_RESETFORSEEK = 405,
    SUBTITLE_UPDATEVIDEOPTS = 406,
    SUBTITLE_GETTOTALTRACKS=407,
    SUBTITLE_GETTYPE=408,
    SUBTITLE_GETLANGUAGE=409,
    SUBTITLE_GETSUBTYPE=410,
    SUBTITLE_GETSUBPID=411,
    SUBTITLE_SETPAGEID=412,
    SUBTITLE_GETANCPAGE=413,
    SUBTITLE_SETANCPAGEID=414,
    SBUTITLE_SETCHANNELID=415,
    SUBTITLE_SETCLOSECAPTIONVFMT=416,
    SUBTITLE_TTCONTROL=417,
    SUBTITLE_TTGOHOME=418,
    SUBTITLE_TTNEXTPAGE=419,
    SUBTITLE_TTNEXTSUBPAGE=420,
    SUBTITLE_TTGOTOPAGE=421,

    SUBTITLE_USERDATAOPEN=422,
    SUBTITLE_USERDATACLOSE=423,
    SUBTITLE_PREPAREWRITINGQUEUE=424,
    SUBTITLE_SETCALLBACK=425,
    SUBTITLE_REMOVECALLBACK=426,
    SUBTITLE_SETFALLCALLBACK=427,
    SUBTITLE_SETSUBTYPE = 428,
    SUBTITLE_SETRENDERTYPE = 429,
    SUBTITLE_SETPIPID = 430,
    SUBTITLE_CMD_MAX                        = 499,
}subtitlecmd_t;

#endif  //AMLOGIC_SUBTITLECMD_H
