#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief Misc functions
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-05: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <am_debug.h>
#include "am_misc.h"
/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Static functions
 ***************************************************************************/
int (*Write_Sysfs_ptr)(const char *path, char *value);
int (*ReadNum_Sysfs_ptr)(const char *path, char *value, int size);
int (*Read_Sysfs_ptr)(const char *path, char *value);

//static int AM_SYSTEMCONTROL_INIT=0;

typedef struct am_rw_sysfs_cb_s
{
	AM_Read_Sysfs_Cb readSysfsCb;
	AM_Write_Sysfs_Cb writeSysfsCb;
}am_rw_sysfs_cb_t;

am_rw_sysfs_cb_t rwSysfsCb = {.readSysfsCb = NULL, .writeSysfsCb = NULL};

typedef struct am_rw_prop_cb_s
{
	AM_Read_Prop_Cb readPropCb;
	AM_Write_Prop_Cb writePropCb;
}am_rw_prop_cb_t;

am_rw_prop_cb_t rwPropCb = {.readPropCb = NULL, .writePropCb = NULL};

#ifdef ANDROID

# define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un*) 0)->sun_path) + strlen ((ptr)->sun_path) )

#endif




/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 注册读写sysfs的回调函数
 * \param[in] fun 回调
 * \return
 *	 - AM_SUCCESS 成功
 *	 - 其他值 错误代码
 */
void AM_RegisterRWSysfsFun(AM_Read_Sysfs_Cb RCb, AM_Write_Sysfs_Cb WCb)
{
#if 0
	assert(RCb && WCb);

	if (!rwSysfsCb.readSysfsCb)
		rwSysfsCb.readSysfsCb = RCb;
	if (!rwSysfsCb.writeSysfsCb)
		rwSysfsCb.writeSysfsCb = WCb;

	AM_DEBUG(1, "AM_RegisterRWSysfsFun !!");
#endif
}

/**\brief 卸载注册
 */
void AM_UnRegisterRWSysfsFun()
{
#if 0
	if (rwSysfsCb.readSysfsCb)
		rwSysfsCb.readSysfsCb = NULL;
	if (rwSysfsCb.writeSysfsCb)
		rwSysfsCb.writeSysfsCb = NULL;
#endif
}

/**\brief 注册读写prop的回调函数
 * \param[in] fun 回调
 * \return
 *	 - AM_SUCCESS 成功
 *	 - 其他值 错误代码
 */
void AM_RegisterRWPropFun(AM_Read_Prop_Cb RCb, AM_Write_Prop_Cb WCb)
{
#if 0
	assert(RCb && WCb);

	if (!rwPropCb.readPropCb)
		rwPropCb.readPropCb = RCb;
	if (!rwPropCb.writePropCb)
		rwPropCb.writePropCb = WCb;

	AM_DEBUG(1, "AM_RegisterRWSysfsFun !!");
#endif
}

/**\brief 卸载prop注册
 */
void AM_UnRegisterRWPropFun()
{
#if 0
	if (rwPropCb.readPropCb)
		rwPropCb.readPropCb = NULL;
	if (rwPropCb.writePropCb)
		rwPropCb.writePropCb = NULL;
#endif
}

/**\brief 向一个文件打印字符串
 * \param[in] name 文件名
 * \param[in] cmd 向文件打印的字符串
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_FileEcho(const char *name, const char *cmd)
{
#if 0
	int fd, len, ret;

	assert(name && cmd);

	if (rwSysfsCb.writeSysfsCb)
	{
		rwSysfsCb.writeSysfsCb(name, cmd);
		return AM_SUCCESS;
	} else
	{
		am_init_syscontrol_api();
		if (Write_Sysfs_ptr != NULL)
		{
			return Write_Sysfs_ptr(name,cmd);
		}
	}

	fd = open(name, O_WRONLY);
	if (fd == -1)
	{
		AM_DEBUG(1, "cannot open file \"%s\"", name);
		return AM_FAILURE;
	}

	len = strlen(cmd);

	ret = write(fd, cmd, len);
	if (ret != len)
	{
		AM_DEBUG(1, "write failed file:\"%s\" cmd:\"%s\" error:\"%s\"", name, cmd, strerror(errno));
		close(fd);
		return AM_FAILURE;
	}

	close(fd);
#endif
	return AM_SUCCESS;
}

/**\brief 读取一个文件中的字符串
 * \param[in] name 文件名
 * \param[out] buf 存放字符串的缓冲区
 * \param len 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_FileRead(const char *name, char *buf, int len)
{
#if 1
	int fd;
    int c = 0;
	//unsigned long val = 0;
	//char bcmd[32];
	//memset(bcmd, 0, 32);
	fd = open(name, O_RDONLY);
	if (fd >= 0) {
		c = read(fd, buf, len);
		if (c > 0) {
			//AM_DEBUG(0, "read sucess val:%s", buf);
		} else {
			//AM_DEBUG(0, "read failed!file %s,err: %s", name, strerror(errno));
		}
		close(fd);
	} else {
		//AM_DEBUG(0, "unable to open file %s,err: %s", name, strerror(errno));
	}

#else
	FILE *fp;
	char *ret;

	assert(name && buf);

	if (rwSysfsCb.readSysfsCb)
	{
		rwSysfsCb.readSysfsCb(name, buf, len);
		return AM_SUCCESS;
	} else
	{
		am_init_syscontrol_api();
		if (ReadNum_Sysfs_ptr != NULL)
		{
			return ReadNum_Sysfs_ptr(name,buf,len);
		}
	}


	fp = fopen(name, "r");
	if (!fp)
	{
		AM_DEBUG(1, "cannot open file \"%s\"", name);
		return AM_FAILURE;
	}

	ret = fgets(buf, len, fp);
	if (!ret)
	{
		AM_DEBUG(1, "read the file:\"%s\" error:\"%s\" failed", name, strerror(errno));
	}

	fclose(fp);
#endif
	return c > 0 ? AM_SUCCESS : AM_FAILURE;
}


/**\brief 向一个prop set字符串
 * \param[in] name 文件名
 * \param[in] cmd 向propset 的字符串
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_PropEcho(const char *name, const char *cmd)
{
#if 0
	int fd, len, ret;

	assert(name && cmd);

	if (rwPropCb.writePropCb)
	{
		rwPropCb.writePropCb(name, cmd);
		return AM_SUCCESS;
	}
#endif
	return AM_SUCCESS;
}

/**\brief 读取一个prop字符串
 * \param[in] name prop名
 * \param[out] buf 存放字符串的缓冲区
 * \param len 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_PropRead(const char *name, char *buf, int len)
{
#if 0
	FILE *fp;
	char *ret;

	assert(name && buf);

	if (rwPropCb.readPropCb)
	{
		rwPropCb.readPropCb(name, buf, len);
		return AM_SUCCESS;
	}
#endif
	return AM_FAILURE;
}

/**\brief 创建本地socket服务
 * \param[in] name 服务名称
 * \param[out] fd 返回服务器socket
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_LocalServer(const char *name, int *fd)
{
#if 0
	struct sockaddr_un addr;
	int s, ret;

	assert(name && fd);

	s = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (s == -1)
	{
		AM_DEBUG(1, "cannot create local socket \"%s\"", strerror(errno));
		return AM_FAILURE;
	}

	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);

	ret = bind(s, (struct sockaddr*)&addr, SUN_LEN(&addr));
	if (ret == -1)
	{
		AM_DEBUG(1, "bind to \"%s\" failed \"%s\"", name, strerror(errno));
		close(s);
		return AM_FAILURE;
	}

	ret = listen(s, 5);
	if (ret == -1)
	{
		AM_DEBUG(1, "listen failed \"%s\" (%s)", name, strerror(errno));
		close(s);
		return AM_FAILURE;
	}

	*fd = s;
#endif
	return AM_SUCCESS;

}

/**\brief 通过本地socket连接到服务
 * \param[in] name 服务名称
 * \param[out] fd 返回socket
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_LocalConnect(const char *name, int *fd)
{
#if 0
	struct sockaddr_un addr;
	int s, ret;

	assert(name && fd);

	s = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (s == -1)
	{
		AM_DEBUG(1, "cannot create local socket \"%s\"", strerror(errno));
		return AM_FAILURE;
	}

	addr.sun_family = AF_LOCAL;
        strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);

        ret = connect(s, (struct sockaddr*)&addr, SUN_LEN(&addr));
        if (ret == -1)
        {
            AM_DEBUG(1, "connect to \"%s\" failed \"%s\"", name, strerror(errno));
            close(s);
		return AM_FAILURE;
        }

        *fd = s;
#endif
        return AM_SUCCESS;
}

/**\brief 通过本地socket发送命令
 * \param fd socket
 * \param[in] cmd 命令字符串
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_LocalSendCmd(int fd, const char *cmd)
{
#if 0
	AM_ErrorCode_t ret;
	int len;

	assert(cmd);

	len = strlen(cmd)+1;

	ret = try_write(fd, (const char*)&len, sizeof(int));
	if (ret != AM_SUCCESS)
	{
		AM_DEBUG(1, "write local socket failed");
		return ret;
	}

	ret = try_write(fd, cmd, len);
	if (ret != AM_SUCCESS)
	{
		AM_DEBUG(1, "write local socket failed");
		return ret;
	}

	AM_DEBUG(2, "write cmd: %s", cmd);
#endif
	return AM_SUCCESS;
}

/**\brief 通过本地socket接收响应字符串
 * \param fd socket
 * \param[out] buf 缓冲区
 * \param len 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_LocalGetResp(int fd, char *buf, int len)
{
#if 0
	AM_ErrorCode_t ret;
	int bytes;

	assert(buf);

	ret = try_read(fd, (char*)&bytes, sizeof(int));
	if (ret != AM_SUCCESS)
	{
		AM_DEBUG(1, "read local socket failed");
		return ret;
	}

	if (len < bytes)
	{
		AM_DEBUG(1, "respond buffer is too small");
		return AM_FAILURE;
	}

	ret = try_read(fd, buf, bytes);
	if (ret != AM_SUCCESS)
	{
		AM_DEBUG(1, "read local socket failed");
		return ret;
	}
#endif
	return AM_SUCCESS;
}


static int in_utf8(unsigned long value, void *arg)
{
#if 0
	int *nchar;
	nchar = arg;
	(*nchar) += value;
#endif
	return 1;
}
/*
static int is_ctrl_code(unsigned long v)
{
	return ((v >= 0x80) && (v <= 0x9f))
		|| ((v >= 0xE080) && (v <= 0xE09f));
}
*/
/* This function traverses a string and passes the value of each character
 * to an optional function along with a void * argument.
 */

static int traverse_string(const unsigned char *p, int len,
		 int (*rfunc)(unsigned long value, void *in), void *arg,char *dest, int *dest_len)
{
#if 0
	unsigned long value;
	int ret;
	while (len) {

		ret = UTF8_getc(p, len, &value);
		if (ret < 0)
		{
			len -- ;
			p++;
			continue;
		}//return -1;
		else if (is_ctrl_code(value))
		{//remove the control codes
			len -= ret;
			p += ret;
			continue;
		}
		else
		{
			int *pos = arg;
			if ((*pos + ret) > *dest_len)
				break;

			char *tp = dest+(*pos);
			memcpy(tp,p,ret);
			len -= ret;
			p += ret;
		}
		if (rfunc) {
			ret = rfunc(ret, arg);
			if (ret <= 0) return ret;
		}
	}
#endif
	return 1;
}


/**\brief 跳过无效UTF8字符
 * \param[in] src 源字符缓冲区
 * \param src_len 源字符缓冲区大小
 * \param[out] dest 目标字符缓冲区
 * \param[in] dest_len 目标字符缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_Check_UTF8(const char *src, int src_len, char *dest, int *dest_len)
{
	//assert(src);
	//assert(dest);
	//assert(dest_len);

	//int ret;
	int nchar;
	nchar = 0;
	/* This counts the characters and does utf8 syntax checking */
	traverse_string((unsigned char*)src, src_len, in_utf8, &nchar,dest, dest_len);

	*dest_len = nchar;
	return AM_SUCCESS;
}

static int am_debug_loglevel = AM_DEBUG_LOGLEVEL_DEFAULT;

void AM_DebugSetLogLevel(int level)
{
	am_debug_loglevel = level;
}

int AM_DebugGetLogLevel()
{
	return am_debug_loglevel;
}
