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
        tFrame stFrame;
        
        if(pstRawDevice->read(&pstRawDevice->stRawInfo,&stFrame) < 0)
        {
            debug("read error!\n");           
        }
        else
        {
            debug("read one frame, addr = %p,size = %u\n",stFrame.frameAddr,stFrame.frameSize);
            pstRawDevice->write(&pstRawDevice->stRawInfo,&stFrame);
        }   
    }
    
    pstRawDevice->close(&pstRawDevice->stRawInfo);
}

