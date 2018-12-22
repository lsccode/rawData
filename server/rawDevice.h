#ifndef __RAW_CONFIG_H__
#define __RAW_CONFIG_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <linux/types.h>
#include <strings.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <sys/mman.h>

#include "rawmsg.h"

#define M_RAW_DEVICE "/dev/sensor-device"
#define M_MEM_DEVICE "/dev/memory_dev"
#define M_MAP_SIZE 399*1024*1024
#define M_DEFAULT_RAW_CONFIG_NMBER (8)
#define M_INIT_FRAME_FREE_NUMB (4)
#define M_MAP_PHY_ADDR (0x9f000000)

typedef struct tagFrame
{
    unsigned int w;
    unsigned int h;
    unsigned int frameSize;
    unsigned int offset;
    char *frameAddr;
    char *phyAddr;
}tFrame;

typedef struct tagFrameArray
{
    unsigned int size;
    unsigned int cap;
    unsigned int getfailed;
    unsigned int addfailed;
    tFrame **ppstFrame;
    char *name;
    pthread_mutex_t mutex;
}tFrameArray;

typedef struct sensor_msg_rawbuf tSensorMsgRawBuf;
typedef struct tagRawInfo
{
    int rawfd;
    int memfd;
    char *memDev;
    char *rawDev;
    unsigned int w;
    unsigned int h;
    unsigned int rawFrameSize;
    unsigned int rawBufNum;
    
    tFrameArray stEmptyArray;
    tFrameArray stUsedArray;
    
    char *mapStartAddr;
}tRawInfo;

typedef struct tagRawOpr
{
    tRawInfo stRawInfo;
    int (*open)(tRawInfo *pstRawInfo);
    int (*read)(tRawInfo *pstRawInfo);
    int (*get)(tRawInfo *pstRawInfo,tFrame **ppstFrame);
    int (*write)(tRawInfo *pstRawInfo,tFrame *pstFrame);
    int (*close)(tRawInfo *pstRawInfo);
    int (*log)(tRawInfo *pstRawInf);
}tRawOpr;


tRawOpr* creatDevice();
tRawOpr* creatVirtualDevice();

#endif
