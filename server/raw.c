#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "rawDevice.h"
#include "logdebug.h"

int main()
{
    unsigned count = 1;
    tRawOpr *pstRawDevice = creatDevice();
    
    if(pstRawDevice->open(&pstRawDevice->stRawInfo) < 0)
    {
        debug("open error!\n");
        return -1;
    }
    
    debug("\n""############################### \n"
          " raw start (%s-%s) \n"
          "###############################\n",__DATE__,__TIME__);
    
    while(1)
    {
        tFrame *pstFrame;
        
        if(pstRawDevice->read(&pstRawDevice->stRawInfo) < 0)
        {
            debug("read error!\n");           
        }
        
        if(pstRawDevice->get(&pstRawDevice->stRawInfo,&pstFrame) < 0)
        {
            debug("read error!\n");           
        }
        
        else
        {
            debug("read one frame, virtual addr (%p), phy addr (%p),size = %u\n",
                  pstFrame->frameAddr,pstFrame->phyAddr,pstFrame->frameSize);
            pstRawDevice->write(&pstRawDevice->stRawInfo,pstFrame);
        }   
        ++count;
        if(count % 100 == 0)
        {
            debug("get %u \n",count);
        }
    }
    debug("\n");
    pstRawDevice->close(&pstRawDevice->stRawInfo);
}

