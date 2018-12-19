#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "rawDevice.h"
#include "logdebug.h"

static int rawOpenDevice(const char *dev)
{
	int fd;
	
	if ((fd = open(dev, O_RDWR | O_SYNC)) == -1) {
		debug("open %s failed, [%s]\n", dev, strerror(errno));
		return -1;
	}
	debug("character device %s opened.\n", dev); 
	
    fflush(stdout);
	
    return fd;
}
static int rawGetFrame(int fd,char *frame)
{
    return 0;
}
static int rawFeedBuf(int fd,char *frame)
{
    return 0;
}
static int rawCloseDevice(int fd)
{
    return 0;
}

static int memmapDevice(tRawInfo *pstRawInfo)
{
    int fd = rawOpenDevice(M_MEM_DEVICE);
    if(fd < 0)
    {
        debug("open mem device error!\n");
        return -1;
    }
    char *addr = mmap(0, M_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr  == (void *) -1) {
		debug("mmap failed, [%s]\n", strerror(errno));
        close(fd);
		return -1;
    }
    
    pstRawInfo->memfd = fd;
    pstRawInfo->mapStartAddr = addr;
    debug("memory mapped at address %p !\n", pstRawInfo->mapStartAddr); 
    fflush(stdout);
    
    return 0;
}
static int closeDevice(tRawInfo *pstRawInfo);
static int openDevice(tRawInfo *pstRawInfo)
{
    struct sensor_io_msg stSensorMsg = {0};
    struct sensor_msg_rawinfo *pstSonsorinfo = (struct sensor_msg_rawinfo *)stSensorMsg.data;
    struct sensor_msg_start   *pstSensorStart = (struct sensor_msg_start *)stSensorMsg.data;
    struct sensor_msg_rawbuf  *pstSensorRawBuf = (struct sensor_msg_rawbuf *)stSensorMsg.data;

    pstRawInfo->devName = M_RAW_DEVICE;
   
    if(memmapDevice(pstRawInfo) < 0)
    { 
        debug("memmapDevice error!\n");
        return -1;
    }
    
    pstRawInfo->rawfd = rawOpenDevice(M_RAW_DEVICE);
    if(pstRawInfo->rawfd < 0)
    {
        debug("open raw device error!\n");
        closeDevice(pstRawInfo);
        return -1;
    }
    
    pstSonsorinfo->frame_w = pstRawInfo->w;
    pstSonsorinfo->frame_h = pstRawInfo->h;
    
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_SET_RAWINFO;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_rawinfo);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}
    
    pstSensorRawBuf->rawbuf_addr = M_MAP_PHY_ADDR;
    pstSensorRawBuf->rawbuf_size = pstRawInfo->rawFrameSize * M_DEFAULT_RAW_CONFIG_NMBER;
    pstSensorRawBuf->queue_num   = M_DEFAULT_RAW_CONFIG_NMBER;
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_SET_RAWBUF;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_rawbuf);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}
    
    pstSensorStart->sensor    = 0;    
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_START;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_start);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}                               
	
    return 0;
}

static int readDevice(tRawInfo *pstRawInfo,tFrame *pstFrame)
{
    struct sensor_msg_rawbuf stMsgRaw;
    
    if(read(pstRawInfo->rawfd,&stMsgRaw,sizeof(stMsgRaw)) < 0)
    {
        debug("read device error!");
    }
    
    if(pstRawInfo->frameFreeNumber > 0)
    {
        struct sensor_io_msg stSensorMsg = {0};
        struct sensor_msg_rawbuf  *pstSensorRawBuf = &pstRawInfo->stzSensorMsgRawBuf[pstRawInfo->frameFreeNumber - 1];
        --pstRawInfo->frameFreeNumber;
        
        stSensorMsg.header.msg_id = SENSOR_IO_MSG_SET_RAWBUF;
        stSensorMsg.header.msg_length = sizeof(struct sensor_msg_rawbuf);
        memcpy(stSensorMsg.data,pstSensorRawBuf,sizeof(struct sensor_msg_rawbuf));
        if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
            debug("failed to set map table: %s\n", strerror(errno));
            return -1;
        }
        
    }
    
    pstFrame->frameAddr = (char *)stMsgRaw.rawbuf_addr;
    pstFrame->frameSize = stMsgRaw.rawbuf_size;
    
    return 0;
}

int writeDevice(tRawInfo *pstRawInfo,tFrame *pstFrame)
{
    unsigned int frameFreeNumber = pstRawInfo->frameFreeNumber;
    tSensorMsgRawBuf stSensorMsgRawBuf = {0};
    
    stSensorMsgRawBuf.rawbuf_addr = pstFrame->frameAddr - pstRawInfo->mapStartAddr + M_MAP_PHY_ADDR;
    stSensorMsgRawBuf.rawbuf_size = pstFrame->frameSize;
    stSensorMsgRawBuf.queue_num   = 1;
    
    pstRawInfo->stzSensorMsgRawBuf[frameFreeNumber] = stSensorMsgRawBuf;  
    ++pstRawInfo->frameFreeNumber;
    
    return 0;
}

static int closeDevice(tRawInfo *pstRawInfo)
{
    int ret = 0;
    
    if(pstRawInfo->memfd > 0)
    {
        if (munmap(pstRawInfo->mapStartAddr, M_MAP_SIZE) == -1) 
        {		
            ret = -1;
            debug("munmap failed, [%s]\n", strerror(errno));
        }
        fsync(pstRawInfo->memfd);
        
        if(close(pstRawInfo->memfd) != 0)
        {
            ret = -1;
            debug("close memfd error!\n");
        }
        pstRawInfo->memfd = 0;
    }

    
    if(pstRawInfo->rawfd)
    {
        if(close(pstRawInfo->rawfd) != 0)
        {
            ret = -1;
            debug("close rawfd error!\n");
        }
    }
    
	return ret;
}

tRawOpr* creatDevice()
{
   tRawOpr *pstRawOpt = (tRawOpr *)calloc(sizeof(tRawOpr),1);
   
   if(NULL == pstRawOpt)
   {
        debug("create device error!\n");
        return NULL;
   }
   pstRawOpt->stRawInfo.w = 672;
   pstRawOpt->stRawInfo.h = 380;
   pstRawOpt->stRawInfo.rawFrameSize = pstRawOpt->stRawInfo.w * pstRawOpt->stRawInfo.h * 2;
   
   pstRawOpt->open  = openDevice;
   pstRawOpt->read  = readDevice;
   pstRawOpt->write = writeDevice;
   pstRawOpt->close = closeDevice;
   
   return pstRawOpt;
}
