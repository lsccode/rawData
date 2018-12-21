#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "rawDevice.h"
#include "logdebug.h"

int main()
{
    tRawOpr *pstRawDevice = creatDevice();
    
    if(pstRawDevice->open(&pstRawDevice->stRawInfo) < 0)
    {
        debug("open error!\n");
        return -1;
    }
    
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
            debug("read one frame, addr = %p,size = %u\n",pstFrame->frameAddr,pstFrame->frameSize);
            pstRawDevice->write(&pstRawDevice->stRawInfo,pstFrame);
        }   
    }
    
    pstRawDevice->close(&pstRawDevice->stRawInfo);
}

