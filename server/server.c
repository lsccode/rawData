#include "ev.h"
#include "netcommon.h"
#include "rawDevice.h"

#define M_MAX_CLIENT_NUMBER (1)
#define M_LOCAL_TEST
typedef struct tagClientfdArray
{
    unsigned int size;
    int szClientFd[M_MAX_CLIENT_NUMBER];
}tClientfdArray;

typedef struct tagServerData
{
    tClientfdArray sztClinet;
    tRawOpr *pstRawDevice;
    struct ev_io *evRawIO;
}tServerData;

typedef struct tagClientData
{
    int start;
    unsigned int bufSize;
    unsigned int bufOffset;
    int sendRawFrameNumber;
    char *pSendBuf;
    tServerData *pstServerData;
    tClientInfo stClientInfo;
    unsigned long long totalsend;
    unsigned long long totaltime; // ms
    struct timespec starttime ;
    struct timespec endtime ;
}tClientData;

//tClientfdArray gstClientArray = {0};
//unsigned long long sum = 0;
//unsigned int count = 0; //100*1000*1000;
//struct timespec starttime = {0, 0};
//struct timespec endtime = {0, 0};
//unsigned int diff = 0;
#ifdef M_LOCAL_TEST
struct ev_io *evIO = NULL;
char *pSendBuf = NULL;
#define M_PACKET_TEST_SIZE (4*1024*1024)
#endif

void passiveClientWrite(struct ev_loop *loop, struct ev_io *watcher, int revents);
//read client 
void passiveClientRead(struct ev_loop *loop, struct ev_io *watcher, int revents){
    char buffer[M_BUFFER_SIZE];
    ssize_t read;
    int index = 0;
    tClientData *pstClientData = watcher->data;
    
    if(EV_ERROR & revents)
    {
        debug("error event in read");
        return;
    }
    
//recv 
    read = recv(watcher->fd, buffer, M_BUFFER_SIZE, 0);
    
    if(read < 0)
    {
        debug("read error");
        return;
    }
    
//client disconnect
    if(read == 0)
    {
        
        tClientData  *pstClientData = (tClientData  *)watcher->data;
        if(NULL != pstClientData)
        {
            debug("%s:%d fd(%d),disconnected!\n", pstClientData->stClientInfo.ipaddr,pstClientData->stClientInfo.port, watcher->fd);
        }
        else
        {
            debug("someone disconnect fd(%d)\n", watcher->fd);
        }
        
#ifdef M_LOCAL_TEST
        evIO = NULL;
#endif
        close(watcher->fd);
        ev_io_stop(loop,watcher);
        pstClientData->pstServerData->sztClinet.size = 0;
        free(watcher->data);
        free(watcher);
#ifndef M_LOCAL_TEST         
        ev_io_stop(loop,pstClientData->pstServerData->evRawIO);
        free(pstClientData->pstServerData->evRawIO);
        pstClientData->pstServerData->pstRawDevice->close(&pstClientData->pstServerData->pstRawDevice->stRawInfo);
#endif
        return;
    }
    else
    {
        tNetMsg *pstNetMsg = (tNetMsg *)buffer;
        if(pstNetMsg->ulMsgType == M_START_RAW)
        {
            debug("get the message ulMsgType :%d\n",pstNetMsg->ulMsgType);
            debug("start send rawdata\n");
            pstNetMsg->ulMsgType = 10000;
            
            pstClientData->start = 1;
            
            //clock_gettime(CLOCK_MONOTONIC, &starttime);
            //ev_io_stop(loop, watcher);
            //ev_io_init(watcher, passiveClientWrite, watcher->fd, EV_WRITE);
            //ev_io_start(loop, watcher);
           
                
        }
        
    }
}

void passiveClientWrite(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    char buffer[32];
    unsigned curSend = 0;
    struct iovec iv[2];
    int sd = 0;
    tClientData *pstClientData = watcher->data;
    curSend = pstClientData->bufSize - pstClientData->bufOffset;
    curSend = curSend > (M_PACKET_SIZE - sizeof(tNetMsg)) ? (M_PACKET_SIZE - sizeof(tNetMsg)) : curSend;
    tNetMsg *pstNetMsg = (tNetMsg *)buffer;
    pstNetMsg->ulMsgLen  = curSend;
    if(pstClientData->bufOffset == 0)
    {              
        pstNetMsg->ulMsgType = M_FRAME_START;  
        debug("send start %u\n",pstClientData->bufOffset);
        clock_gettime(CLOCK_MONOTONIC, &pstClientData->starttime);
    }
    else if(pstClientData->bufOffset + curSend < pstClientData->bufSize)
    {
        //debug("send mid\n");
        pstNetMsg->ulMsgType = M_FRAME_MIDDLE;
    }
    else
    {
        debug("send end %u\n",pstClientData->bufOffset);
        pstNetMsg->ulMsgType = M_FRAME_END;
    }
    
    iv[0].iov_base = buffer;
    iv[0].iov_len  = sizeof(tNetMsg);
    iv[1].iov_base = pstClientData->pSendBuf + pstClientData->bufOffset;
    iv[1].iov_len  = curSend;
    
    //debug("send %d:%d\n", sizeof(tNetMsg),curSend);
    // bug fixed,default send all data one time
    sd = writev(watcher->fd, iv, 2);
    if(sd > 0)
    {
        pstClientData->totalsend += sd;
        pstClientData->bufOffset += curSend;
        //debug("send sd(%d)\n",sd);
    }
    else
    {
        ev_io_stop(loop, watcher);
        ev_io_init(watcher, passiveClientRead, watcher->fd, EV_READ);
        ev_io_start(loop, watcher);
#ifdef M_LOCAL_TEST
        evIO = NULL;
#endif
        debug("write error!\n");
        return;
    } 
    
    if(pstClientData->bufOffset == pstClientData->bufSize)
    {
        clock_gettime(CLOCK_MONOTONIC, &pstClientData->endtime);
        ++pstClientData->sendRawFrameNumber;
        pstClientData->totaltime += 
            (pstClientData->endtime.tv_sec * 1000 + pstClientData->endtime.tv_nsec/(1000*1000)) - 
            (pstClientData->starttime.tv_sec * 1000 + pstClientData->starttime.tv_nsec/(1000*1000)); 
        ev_io_stop(loop, watcher);
        ev_io_init(watcher, passiveClientRead, watcher->fd, EV_READ);
        ev_io_start(loop, watcher);
       // debug("send one frame %u:%u\n",pstClientData->bufOffset,pstClientData->bufSize);
    }
    
    if(pstClientData->sendRawFrameNumber % 25 && pstClientData->bufOffset == pstClientData->bufSize)
    {
        double sec = pstClientData->totaltime*1.0/1000;
        debug("total send (%llu MB ~= %llu B),Speed %0.2f MB/s \n",
              pstClientData->totalsend/(1024*1024),pstClientData->totalsend,
              pstClientData->totalsend/(sec*1024*1024));
    }   
}

void rawFdRead(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    struct ev_io *w_client = (struct ev_io *)watcher->data;
    tFrame stFrame = {0};
    
    if(w_client && w_client->data)
    {
        tClientData  *pstClientData = (tClientData  *)w_client->data;
        tRawOpr *pstRawDevice = pstClientData->pstServerData->pstRawDevice;
        
        pstRawDevice->read(&pstRawDevice->stRawInfo,&stFrame);
        
        if(pstClientData->start)
        {
            ev_io_stop(loop, w_client);
            ev_io_init(w_client, passiveClientWrite, w_client->fd, EV_WRITE);
            ev_io_start(loop, w_client);
            
            pstClientData->pSendBuf = stFrame.frameAddr;
            pstClientData->bufSize  = stFrame.frameSize;
            pstClientData->bufOffset = 0;
        }      
    }
 
    return;
}
//accept server callback
void serverAcceptRead(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sd;
    int index = 0;
    tServerData *pstServerData = (tServerData *)watcher->data;
    
    struct ev_io *w_client = (struct ev_io*) malloc (sizeof(struct ev_io));
    tClientData  *pstClientData = (tClientData*)malloc (sizeof(tClientData));
    
    if(EV_ERROR & revents)
    {
        debug("error event in accept");
        return;
    }
    
    if(NULL == w_client || NULL == pstClientData)
    {
        debug("create client error!\n");
        return;
    }
    
    client_sd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_sd < 0)
    {
        debug("accept error");
        return;
    }
    debug("error event in accept");
    if(pstServerData->sztClinet.size != 0)
    {
        tNetMsg stNetMsg;
        stNetMsg.ulMsgType = M_SERVER_BUSY;
        stNetMsg.ulMsgLen  = 0;       
        send(client_sd,&stNetMsg,sizeof(stNetMsg),0);
        free(w_client);
        close(client_sd);       
        debug("server busy!\n");
        
        return;
    }
    debug("error event in accept");
    inet_ntop(AF_INET,&(client_addr.sin_addr),pstClientData->stClientInfo.ipaddr,sizeof(pstClientData->stClientInfo.ipaddr)); 
    pstClientData->stClientInfo.port = ntohs(client_addr.sin_port);
    pstClientData->pstServerData = pstServerData;
    
    w_client->data = pstClientData;
    ev_io_init(w_client, passiveClientRead, client_sd, EV_READ);       
    ev_io_start(loop, w_client);

#ifndef M_LOCAL_TEST
    if(pstServerData->evRawIO == NULL)
    {
        pstServerData->evRawIO = (struct ev_io*) malloc (sizeof(struct ev_io));
        pstServerData->pstRawDevice->open(&pstServerData->pstRawDevice->stRawInfo);
        
        pstServerData->evRawIO->data = w_client;
        ev_io_init(pstServerData->evRawIO, rawFdRead, pstServerData->pstRawDevice->stRawInfo.rawfd, EV_READ);       
        ev_io_start(loop, pstServerData->evRawIO);
    }
    
#else
    evIO = w_client;
#endif    
    
    
    pstServerData->sztClinet.size = 1;
    debug("%s:%d fd(%d) connected!\n", pstClientData->stClientInfo.ipaddr, pstClientData->stClientInfo.port,w_client->fd);
}

int createTcpServer()
{
    int server_sockfd;
    int server_len;
    struct sockaddr_in server_address;
    
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    int reuseFlag = 1;
    socklen_t socklen = sizeof(reuseFlag);
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseFlag, socklen);

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVICE_PORT);
    server_len = sizeof(server_address);
    if(bind(server_sockfd, (struct sockaddr *)&server_address, server_len) < 0)
    {
        debug("bind ,%d %s failed\n",errno,strerror(errno));
    }

    listen(server_sockfd, 5);
    return server_sockfd;
}

void sigpipe(int sig)
{
    debug("server recv signal=%d\n",sig);
    return;
}
void signalCallback(EV_P_ ev_signal *w, int revents)
{
	//debug("server recv signal=%d\n",sig);
	// this causes the innermost ev_run to stop iterating
	//ev_break (EV_A_ EVBREAK_ONE);
	if(w->signum == SIGINT)
	{
		debug("server recv SIGINT(%d)\n",w->signum);
        exit(0);
	}
    
    if(w->signum == SIGPIPE)
    {
        debug("server recv SIGPIPE(%d)\n",w->signum);
    }
}
#ifdef M_LOCAL_TEST
void getRawData (EV_P_ ev_timer *w, int revents)
{
    if(evIO && evIO->data)
    {
        tClientData  *pstClientData = (tClientData  *)evIO->data;
        if(pstClientData->start)
        {          
            ev_io_stop(loop, evIO);
            ev_io_init(evIO, passiveClientWrite, evIO->fd, EV_WRITE);
            ev_io_start(loop, evIO);
            
            pstClientData->pSendBuf = pSendBuf;
            pstClientData->bufSize  = M_PACKET_TEST_SIZE;
            pstClientData->bufOffset = 0;

        }
    }
    
    return;
}
#endif
int main()
{
    struct ev_io evServer;
    tServerData stServerData = {0};
    
    int serverfd = createTcpServer();
    struct ev_loop *loop = ev_default_loop(0);

    stServerData.pstRawDevice = creatDevice();
    if(stServerData.pstRawDevice == NULL)
    {
        debug("create device error!\n");
        return -1;
    }
       
    evServer.data = &stServerData;
    ev_io_init(&evServer, serverAcceptRead, serverfd, EV_READ);
    ev_io_start(loop, &evServer);
    
    ev_signal evsignalint;
	ev_signal_init(&evsignalint, signalCallback,SIGINT);
	ev_signal_start (loop, &evsignalint);
    
    ev_signal evsignalpipe;
	ev_signal_init(&evsignalpipe, signalCallback,SIGPIPE);
	ev_signal_start (loop, &evsignalpipe);

#ifdef M_LOCAL_TEST
    ev_timer evtimeout;
    evtimeout.data = NULL;
    ev_timer_init (&evtimeout, getRawData, 0.001, 0.040);
	ev_timer_start (loop, &evtimeout);
    pSendBuf = (char *)malloc(M_PACKET_TEST_SIZE);
#endif

    ev_run (loop, 0);
    
    return 0;
}




