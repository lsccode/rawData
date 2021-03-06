#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "rawDevice.h"
#include "logdebug.h"

static int initArray(tFrameArray *pstFrameArray,char *name,unsigned cap,unsigned int create)
{
    if(NULL == pstFrameArray)
    {
        debug("pstFrameArray (%p),error!\n",pstFrameArray);
        return -1;
    }
    
   tFrame **ppstFrame = (tFrame **)calloc(cap,sizeof(tFrame *));
    
    if(NULL == ppstFrame)
    {
        debug("calloc ppstFrame (%p) error!\n",ppstFrame);
        return -1;
    }
    
    if(create)
    {
        int index = 0;
        for(index = 0; index < cap; ++index)
        {
            ppstFrame[index] = (tFrame *)calloc(1,sizeof(tFrame));
            if(ppstFrame[index] == NULL) 
            {
                debug("create ppstFrame[%u] = (%p),error!\n",index,ppstFrame[index]);
                return -1;  // return memory leak
            }
        }
        pstFrameArray->size = cap;
    }
    else
    {
        pstFrameArray->size = 0;
    }

    pstFrameArray->cap = cap;
    pstFrameArray->ppstFrame = ppstFrame;
    pstFrameArray->name = name;
    pthread_mutex_init(&pstFrameArray->mutex,NULL);
    
    return 0;
}

static int addTOArray(tFrameArray *pstFrameArray,tFrame *pstFrame)
{
    if(NULL == pstFrameArray || NULL == pstFrame)
    {
        debug("pstFrameArray (%p),pstFrame (%p) error!\n",pstFrameArray,pstFrame);
        return -1;
    }
    
    if(pstFrameArray->size == pstFrameArray->cap)
    {
        //debug("add to (%s) size(%u),cap(%u),full error!\n",pstFrameArray->name,pstFrameArray->size,pstFrameArray->cap);
        ++pstFrameArray->addfailed;
        return -1;
    }
    pthread_mutex_lock(&pstFrameArray->mutex);
    pstFrameArray->ppstFrame[pstFrameArray->size] = pstFrame;
    ++pstFrameArray->size;
    pthread_mutex_unlock(&pstFrameArray->mutex);
    
    return 0;   
}

static int getFromArray(tFrameArray *pstFrameArray,tFrame **ppstFrame,int random)
{
    if(NULL == pstFrameArray || NULL == ppstFrame)
    {
        *ppstFrame = NULL;
        debug("pstFrameArray (%p),ppstFrame (%p) error!\n",pstFrameArray,ppstFrame);
        return -1;
    }
    
    if(pstFrameArray->size == 0)
    {       
        *ppstFrame = NULL;
        ++pstFrameArray->getfailed;
        //debug("get from (%s) size (%d),empty error!\n",pstFrameArray->name,pstFrameArray->size);
        return -1;
    }
    
    pthread_mutex_lock(&pstFrameArray->mutex);
    if(random)
    {
        *ppstFrame = pstFrameArray->ppstFrame[pstFrameArray->size - 1];
        --pstFrameArray->size;
    }
    else
    {
        *ppstFrame = pstFrameArray->ppstFrame[0];
        --pstFrameArray->size;
        memmove(pstFrameArray->ppstFrame,&pstFrameArray->ppstFrame[1],pstFrameArray->size * sizeof(tFrame*));
    }
    pthread_mutex_unlock(&pstFrameArray->mutex);  
    return 0;   
}

static int openmem(tRawInfo *pstRawInfo)
{
    int fd = open(pstRawInfo->memDev, O_RDWR | O_SYNC);
    if(fd < 0)
    {
        debug("open (%s) device error(%s:%d)!\n",pstRawInfo->memDev,strerror(errno),errno);
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
    
    fflush(stdout);
    debug("open (%s) succeed ,fd(%d) mapped address(%p)\n",pstRawInfo->memDev,pstRawInfo->memfd,pstRawInfo->mapStartAddr); 
 
    return 0;
}
static int closeDevice(tRawInfo *pstRawInfo);
static int openDevice(tRawInfo *pstRawInfo)
{
    struct sensor_io_msg stSensorMsg = {0};
    struct sensor_msg_rawinfo *pstSonsorinfo = (struct sensor_msg_rawinfo *)stSensorMsg.data;
    struct sensor_msg_start   *pstSensorStart = (struct sensor_msg_start *)stSensorMsg.data;
    struct sensor_msg_rawbuf  *pstSensorRawBuf = (struct sensor_msg_rawbuf *)stSensorMsg.data;

    pstRawInfo->memDev = M_MEM_DEVICE;
    pstRawInfo->rawDev = M_RAW_DEVICE;
   
    if(openmem(pstRawInfo) < 0)
    { 
        debug("memmapDevice error!\n");
        return -1;
    }
    
    pstRawInfo->rawfd = open(pstRawInfo->rawDev, O_RDWR | O_SYNC);
    if(pstRawInfo->rawfd < 0)
    {
        debug("open (%s) device error(%s:%d)!\n",pstRawInfo->rawDev,strerror(errno),errno);
        closeDevice(pstRawInfo);
        return -1;
    }
    debug("open (%s) succeed ,fd(%d)\n",pstRawInfo->rawDev,pstRawInfo->rawfd);
    
    pstSonsorinfo->frame_w = pstRawInfo->w;
    pstSonsorinfo->frame_h = pstRawInfo->h;
    
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_SET_RAWINFO;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_rawinfo);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}
    
    pstSensorRawBuf->rawbuf_addr = M_MAP_PHY_ADDR;
    pstSensorRawBuf->rawbuf_size = pstRawInfo->rawFrameSize * pstRawInfo->rawBufNum;
    pstSensorRawBuf->queue_num   = pstRawInfo->rawBufNum;
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_SET_RAWBUF;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_rawbuf);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}

    pstSensorStart->sensor    = SENSOR_TYPE_A;    
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_START;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_start);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}                               

    return 0;
}

static int readDevice(tRawInfo *pstRawInfo)
{
    struct sensor_msg_rawbuf stMsgRaw;
    tFrame *pstFrame = NULL;
    
    if(NULL == pstRawInfo)
    {
        debug("pstRawInfo (%p),error!\n",pstRawInfo);
        return -1;
    }
    
    if(read(pstRawInfo->rawfd,&stMsgRaw,sizeof(stMsgRaw)) < 0)
    {
        debug("read device error (%s:%d)!\n",strerror(errno),errno);
		return -1;
    }
    
    if( getFromArray(&pstRawInfo->stEmptyArray,&pstFrame,1) < 0 )
    {
        debug("get from %s error!\n",pstRawInfo->stEmptyArray.name);
        return -1;
    }   
    
    pstFrame->phyAddr   =  (char *)stMsgRaw.rawbuf_addr;
    pstFrame->frameAddr = pstFrame->phyAddr - (char *)M_MAP_PHY_ADDR + pstRawInfo->mapStartAddr;
    pstFrame->frameSize = stMsgRaw.rawbuf_size;
    
    if(addTOArray(&pstRawInfo->stUsedArray,pstFrame) < 0)
    {
        debug("add to %s error!\n",pstRawInfo->stUsedArray.name);
        return -1;
    }
    
    return 0;
}

static int writeDevice(tRawInfo *pstRawInfo,tFrame *pstFrame)
{
    struct sensor_io_msg stSensorMsg = {0};
    struct sensor_msg_rawbuf  *pstSensorRawBuf = (struct sensor_msg_rawbuf  *)stSensorMsg.data;
    
    if(NULL == pstRawInfo || NULL == pstFrame)
    {
        debug("pstRawInfo (%p),pstFrame (%p) error!\n",pstRawInfo,pstFrame);
        return -1;
    }
    
    pstSensorRawBuf->rawbuf_addr = (unsigned int)pstFrame->phyAddr;
    pstSensorRawBuf->rawbuf_size = pstFrame->frameSize;
    pstSensorRawBuf->queue_num   = 1;
    stSensorMsg.header.msg_id = SENSOR_IO_MSG_SET_RAWBUF;
    stSensorMsg.header.msg_length = sizeof(struct sensor_msg_rawbuf);
    if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &stSensorMsg) < 0 ) {
        debug("failed to set map table: %s\n", strerror(errno));
        return -1;
    }
    pstFrame->frameSize = 0;
    pstFrame->phyAddr   = NULL;
    pstFrame->offset    = 0;
    if(addTOArray(&pstRawInfo->stEmptyArray,pstFrame) < 0)
    {
        debug("add to %s error!\n",pstRawInfo->stUsedArray.name);
        return -1;
    }   
    
    return 0;
}

static int getFrame(tRawInfo *pstRawInfo,tFrame **ppstFrame)
{
    if(NULL == pstRawInfo || NULL == ppstFrame)
    {
        debug("pstRawInfo (%p),ppstFrame (%p),error!",pstRawInfo,ppstFrame);
        return -1;
    }
    
    tFrame *pstFrame = NULL;
    if( getFromArray(&pstRawInfo->stUsedArray,&pstFrame,0) < 0 )
    {
        debug("get from (%s) error!\n",pstRawInfo->stUsedArray.name);
        return -1;
    }
    
    *ppstFrame = pstFrame;
    
    return 0;   
}

static int closeDevice(tRawInfo *pstRawInfo)
{
    int ret = 0;
    
    struct sensor_io_msg sensor_msg;
	struct sensor_msg_stop msg_stop;
    tFrame *pstFrame = NULL;
    
	sensor_msg.header.msg_id = SENSOR_IO_MSG_STOP;
	sensor_msg.header.msg_length = sizeof(struct sensor_msg_stop);

	msg_stop.sensor = SENSOR_TYPE_A;
	memcpy(sensor_msg.data, &msg_stop, sizeof(struct sensor_msg_stop));

	if (ioctl(pstRawInfo->rawfd, SENSOR_IO_MSG_CMD, &sensor_msg) < 0 ) {
		debug("failed to set map table: %s\n", strerror(errno));
		return -1;
	}
    
    debug("stop sensor (%d)\n",msg_stop.sensor);
    
    if(pstRawInfo->rawfd)
    {
        if(close(pstRawInfo->rawfd) != 0)
        {
            ret = -1;
            debug("close rawfd error!\n");
        }
        debug("close rawfd (%d)\n",pstRawInfo->rawfd);
        pstRawInfo->rawfd = -1;
    }
    
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
        debug("close memfd (%d)\n",pstRawInfo->memfd);
        
        pstRawInfo->memfd = -1;
    }
    
    while(!getFromArray(&pstRawInfo->stUsedArray,&pstFrame,1))
    {
        if(addTOArray(&pstRawInfo->stEmptyArray,pstFrame) < 0)
        {
            debug("closed add to empty error!\n");
        }
    }
    
    debug("close device ok!\n");
	return ret;
}

static int rawlog(tRawInfo *pstRawInf)
{
    debug("used  number (%u:%u), "
          "empty number (%u:%u) \n",
          pstRawInf->stUsedArray.cap,pstRawInf->stUsedArray.size,
          pstRawInf->stEmptyArray.cap,pstRawInf->stEmptyArray.size  
         );
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
   pstRawOpt->stRawInfo.rawBufNum    = 60;
   
   initArray(&pstRawOpt->stRawInfo.stEmptyArray,"empty array",pstRawOpt->stRawInfo.rawBufNum,1);
   initArray(&pstRawOpt->stRawInfo.stUsedArray,"used array",pstRawOpt->stRawInfo.rawBufNum,0);
   
   pstRawOpt->open     = openDevice;
   pstRawOpt->read     = readDevice;
   pstRawOpt->get      = getFrame; 
   pstRawOpt->write    = writeDevice;
   pstRawOpt->close    = closeDevice;
   pstRawOpt->log      = rawlog;
   
   return pstRawOpt;
}
