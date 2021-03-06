#include <pthread.h>

#include "ev.h"
#include "netcommon.h"
#include "rawDevice.h"

#define M_MAX_CLIENT_NUMBER (1)
//#define M_LOCAL_TEST

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
    struct ev_loop *loop;
}tServerData;

typedef struct tagClientData
{
    int start;
    int sendFrameNumber;
    tFrame *pstFrame;
    tServerData *pstServerData;
    tClientInfo stClientInfo;
    unsigned long long totalsend;
    unsigned long long totaltime; // ms
    struct timespec starttime ;
    struct timespec endtime ;
    int errcode;
}tClientData;

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
        
        
        ev_io_stop(loop,watcher);
        close(watcher->fd);
        pstClientData->pstServerData->sztClinet.size = 0;
        free(watcher->data);
        free(watcher);
        
        ev_io_stop(loop,pstClientData->pstServerData->evRawIO);
        pstClientData->pstServerData->pstRawDevice->close(&pstClientData->pstServerData->pstRawDevice->stRawInfo);
        free(pstClientData->pstServerData->evRawIO);      
        pstClientData->pstServerData->evRawIO = NULL;
        return;
    }
    else
    {
        tNetMsg *pstNetMsg = (tNetMsg *)buffer;
        if(pstNetMsg->ulMsgType == M_START_RAW)
        {
            pstNetMsg->ulMsgType = 10000;            
            pstClientData->start = 1;            
            clock_gettime(CLOCK_MONOTONIC, &pstClientData->starttime); 
            pstClientData->pstServerData->sztClinet.size = 1;
            
            debug("start send rawdata\n");
        }
        
    }
}

void passiveClientWrite(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    if(watcher->data == NULL)
    {
        debug("wathch context error!\n");
        return;
    }
    
    int sd = 0;
    unsigned curSend = 0;
    tClientData *pstClientData = (tClientData *)watcher->data;
    tFrame *pstFrame = NULL;
    
    if(NULL == pstClientData->pstServerData || NULL == pstClientData->pstServerData->pstRawDevice)
    {
        debug("device context error!\n");
        return;
    }
    tRawOpr *pstRawDevice = pstClientData->pstServerData->pstRawDevice;
    
    if(pstClientData->pstFrame == NULL)
    {

        if(pstRawDevice->get(&pstRawDevice->stRawInfo,&pstClientData->pstFrame) < 0)
        {
            //if(pstRawDevice->stRawInfo.stUsedArray.getfailed % 100 == 0)
                debug("client get from (%s) error,failed count(%u) !\n",
                      pstRawDevice->stRawInfo.stUsedArray.name,pstRawDevice->stRawInfo.stUsedArray.getfailed);
            //usleep(20);
            ev_io_stop(loop, watcher);
            ev_io_init(watcher, passiveClientRead, watcher->fd, EV_READ);
            ev_io_start(loop, watcher);
            return;
        }              
    }
    
    pstFrame = pstClientData->pstFrame;
    curSend = pstFrame->frameSize - pstFrame->offset;
    //curSend = curSend > M_PACKET_SIZE ? M_PACKET_SIZE : curSend;
    
    sd = write(watcher->fd,pstFrame->frameAddr + pstFrame->offset,curSend);
    if(sd > 0)
    {
        pstClientData->totalsend += sd;
        pstFrame->offset         += sd;
    }
    else
    {
        
        ev_io_stop(loop, watcher);
        ev_io_init(watcher, passiveClientRead, watcher->fd, EV_READ);
        ev_io_start(loop, watcher);
        pstClientData->errcode = errno;
        debug("write error %s:%d, offset (%d),size (%u),curSend (%u)!\n",
              strerror(errno),errno,pstFrame->offset,pstFrame->frameSize,curSend);
        return;
    }
    
    if(pstFrame->offset == pstFrame->frameSize)
    {    
        ++pstClientData->sendFrameNumber;
        if(pstClientData->sendFrameNumber%100 == 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &pstClientData->endtime);
            pstClientData->totaltime = 
                (pstClientData->endtime.tv_sec * 1000 + pstClientData->endtime.tv_nsec/(1000*1000)) - 
                (pstClientData->starttime.tv_sec * 1000 + pstClientData->starttime.tv_nsec/(1000*1000));
            double sec = pstClientData->totaltime*1.0/1000;
            debug("\ntotal send (%llu MB ~= %llu B),  "
                  "offset (%u:%u),  "
                  "%u/%.2f ~= %.2f MB/s, "
                  "%u/%.2f ~= %.2f fps\n",
                  pstClientData->totalsend/(1024*1024),pstClientData->totalsend,                 
                  pstFrame->frameSize,pstFrame->offset,
                  pstClientData->totalsend/(1024*1024),sec,pstClientData->totalsend/(sec*1024*1024),
                  pstClientData->sendFrameNumber,sec,pstClientData->sendFrameNumber/sec);
            pstRawDevice->log(&pstRawDevice->stRawInfo);
        }
        
        //debug("write a raw frame size (%u),size (%u)\n",pstFrame->frameSize,pstFrame->offset);
        if(pstRawDevice->write(&pstRawDevice->stRawInfo,pstClientData->pstFrame) < 0)
        {
            debug("write a raw frame error!\n");
        }
        
        pstClientData->pstFrame = NULL;
    }    
}

void rawFdRead(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    struct ev_io *w_client = (struct ev_io *)watcher->data;
   
    //debug("read it haha\n");
    if(w_client && w_client->data)
    {
        tClientData  *pstClientData = (tClientData  *)w_client->data;
        tRawOpr *pstRawDevice = pstClientData->pstServerData->pstRawDevice;
        
        if(pstRawDevice->read(&pstRawDevice->stRawInfo) < 0)
        {
            debug("client read device error!\n");
            return;
        }

        if(pstClientData->start)
        {
            if((w_client->events & EV_WRITE) || (pstClientData->errcode != 0))
            {
                return;
            }
            ev_io_stop(loop, w_client);
            ev_io_init(w_client, passiveClientWrite, w_client->fd, EV_WRITE);
            ev_io_start(loop, w_client);
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
    
    struct ev_io *w_client = (struct ev_io*) calloc (1,sizeof(struct ev_io));
    tClientData  *pstClientData = (tClientData*)calloc (1,sizeof(tClientData));
    
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
    
    inet_ntop(AF_INET,&(client_addr.sin_addr),pstClientData->stClientInfo.ipaddr,sizeof(pstClientData->stClientInfo.ipaddr)); 
    pstClientData->stClientInfo.port = ntohs(client_addr.sin_port);
    pstClientData->pstServerData = pstServerData;
    if(pstServerData->sztClinet.size != 0)
    {
        tNetMsg stNetMsg;
        stNetMsg.ulMsgType = M_SERVER_BUSY;
        stNetMsg.ulMsgLen  = 0;       
        send(client_sd,&stNetMsg,sizeof(stNetMsg),0);
        free(w_client);
        close(client_sd);       
        debug("server busy, (%s:%d) fd (%d) closed!\n",
              pstClientData->stClientInfo.ipaddr, pstClientData->stClientInfo.port,client_sd);
        
        return;
    }

    {
        int recvbuff = 2*1024*1024;
        if(setsockopt(client_sd, SOL_SOCKET, SO_SNDBUF, (const char*)&recvbuff, sizeof(int)) == -1)
        {
            debug("");
        }
        
        if(setsockopt(client_sd, SOL_SOCKET, SO_RCVBUF, (const char*)&recvbuff, sizeof(int)) == -1)
        {
            debug("");
        } 
    }
    
    w_client->data = pstClientData;
    ev_io_init(w_client, passiveClientRead, client_sd, EV_READ);       
    ev_io_start(loop, w_client);
    debug("%s:%d fd(%d) connected!\n", pstClientData->stClientInfo.ipaddr, pstClientData->stClientInfo.port,w_client->fd);

    if(pstServerData->evRawIO == NULL)
    {
        pstServerData->evRawIO = (struct ev_io*) malloc (sizeof(struct ev_io));
        if(pstServerData->pstRawDevice->open(&pstServerData->pstRawDevice->stRawInfo) < 0)
        {
            debug("client open device error!\n");
            free(pstServerData->evRawIO);
            pstServerData->evRawIO = NULL;
            return;
        }
        
        pstServerData->evRawIO->data = w_client;
        ev_io_init(pstServerData->evRawIO, rawFdRead, pstServerData->pstRawDevice->stRawInfo.rawfd, EV_READ);       
        ev_io_start(loop, pstServerData->evRawIO);
    }      
    
    pstServerData->sztClinet.size = 1;
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
    {
        int recvbuff = 2*1024*1024;
        if(setsockopt(server_sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&recvbuff, sizeof(int)) == -1)
        {
            debug("");
        }
        
        if(setsockopt(server_sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&recvbuff, sizeof(int)) == -1)
        {
            debug("");
        } 
    }

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

void getRawData (EV_P_ ev_timer *w, int revents)
{
    int rc;
    uint64_t buf = 1;
    
    tServerData *pstServerData = (tServerData *)w->data;    
    tRawOpr *pstRawDevice = pstServerData->pstRawDevice;
    
    if(pstServerData->sztClinet.size)
    {
        rc = write(pstRawDevice->stRawInfo.rawfd, &buf, sizeof(buf));
        if(rc != 8)
        {
            debug("write %s:%d\n",strerror(errno),errno);
        }
        //debug("signal one packet!\n");
    }

    return;
}

int main()
{
    struct ev_io evServer;
    tServerData stServerData = {0};
    
    int serverfd = createTcpServer();
    struct ev_loop *loop = ev_default_loop(0);

    stServerData.loop = loop;
#ifdef M_LOCAL_TEST
    stServerData.pstRawDevice = creatVirtualDevice();
#else
    stServerData.pstRawDevice = creatDevice();
#endif
    
    if(stServerData.pstRawDevice == NULL)
    {
        debug("create device error!\n");
        return -1;
    }
    
    debug("server start (%s-%s)\n",__DATE__,__TIME__);
       
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
    evtimeout.data = &stServerData;
    ev_timer_init (&evtimeout, getRawData, 0.01, 0.030);
	ev_timer_start (loop, &evtimeout);
    
#endif

    ev_run (loop, 0);
    
    return 0;
}




