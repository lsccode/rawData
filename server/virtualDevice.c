#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>

#include "rawDevice.h"
#include "logdebug.h"

#define M_FRAME_W (1280)
#define M_FRAME_H (720)
#define M_FRAME_SIZE (M_FRAME_W*M_FRAME_H*3)

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
            tFrame *pstFrame = (tFrame *)calloc(1,sizeof(tFrame));
            char *frameAddr = (char *)calloc(M_FRAME_SIZE,sizeof(char));
            
            if(NULL == pstFrame || NULL == frameAddr)
            {
                debug("create error!\n");
                return -1;
            }
            
            pstFrame->w = M_FRAME_W;
            pstFrame->h = M_FRAME_H;            
            pstFrame->frameSize = 0;
            pstFrame->offset    = 0;
            pstFrame->frameAddr = frameAddr;
            pstFrame->phyAddr   = NULL;
            
            ppstFrame[index] = pstFrame;          
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
        //debug("(%s) full,size (%u),cap(%u)!\n",pstFrameArray->name,pstFrameArray->size,pstFrameArray->cap);
        ++pstFrameArray->addfailed;
        return -1;
    }
    
    pstFrameArray->ppstFrame[pstFrameArray->size] = pstFrame;
    ++pstFrameArray->size;
    
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
        //debug("(%s) empty,size (%u)!\n",pstFrameArray->name);
        return -1;
    }
    
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
    
    return 0;   
}

static int closeDevice(tRawInfo *pstRawInfo);
static int openDevice(tRawInfo *pstRawInfo)
{   
    pstRawInfo->rawfd = eventfd(0,0);
    
    if(pstRawInfo->rawfd < 0)
    {
        debug("open (%s) error!\n",pstRawInfo->rawDev);
        return -1;
    }
    
    return 0;
}

static int readDevice(tRawInfo *pstRawInfo)
{   
    int rc;
    uint64_t buffer;
    tFrame *pstFrame = NULL;
    if(NULL == pstRawInfo)
    {
        debug("pstRawInfo (%p),error!\n",pstRawInfo);
        return -1;
    }      
    
   // debug("read virtual device ...\n");
    rc = read(pstRawInfo->rawfd, &buffer, sizeof(buffer));
    if(rc < 0)
    {
        debug("read %s:%d!\n",strerror(errno),errno);
        return -1;
    }
    
    if( getFromArray(&pstRawInfo->stEmptyArray,&pstFrame,1) < 0 )
    {
        debug("get from %s error!\n",pstRawInfo->stEmptyArray.name);
        return -1;
    }
   
    pstFrame->frameSize = M_FRAME_SIZE;
    pstFrame->offset    = 0;
    
    if(addTOArray(&pstRawInfo->stUsedArray,pstFrame) < 0)
    {
        debug("add to %s error!\n",pstRawInfo->stUsedArray.name);
        return -1;
    }
    
//    debug("add to (%s) (%d)\n",pstRawInfo->stUsedArray.name,pstRawInfo->stUsedArray.size);
    
    return 0;
}

static int writeDevice(tRawInfo *pstRawInfo,tFrame *pstFrame)
{ 
    
    if(NULL == pstRawInfo || NULL == pstFrame)
    {
        debug("pstRawInfo (%p),pstFrame (%p) error!\n",pstRawInfo,pstFrame);
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
        //debug("get from (%s) error!\n",pstRawInfo->stUsedArray.name);
        return -1;
    }
    
    *ppstFrame = pstFrame;
    
    return 0;   
}

static int rawlog(tRawInfo *pstRawInf)
{
    debug("used  number (%u:%u), "
          "empty number (%u:%u) \n",
          pstRawInf->stUsedArray.cap,pstRawInf->stUsedArray.size,
          pstRawInf->stEmptyArray.cap,pstRawInf->stEmptyArray.size  
         );
}

static int closeDevice(tRawInfo *pstRawInfo)
{   
    close(pstRawInfo->rawfd);
	return 0;
}

tRawOpr* creatVirtualDevice()
{
    tRawOpr *pstRawOpt = (tRawOpr *)calloc(sizeof(tRawOpr),1);
    
    if(NULL == pstRawOpt)
    {
        debug("create device error!\n");
        return NULL;
    }
    pstRawOpt->stRawInfo.w = M_FRAME_W;
    pstRawOpt->stRawInfo.h = M_FRAME_H;
    pstRawOpt->stRawInfo.rawFrameSize = pstRawOpt->stRawInfo.w * pstRawOpt->stRawInfo.h * 2;
    
    pstRawOpt->stRawInfo.rawBufNum    = M_MAP_SIZE/pstRawOpt->stRawInfo.rawFrameSize;
    
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
