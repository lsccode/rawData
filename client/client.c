#include "ev.h"
#include "netcommon.h"
#include <pthread.h>

#define M_FRAME_RATE (25)
#define M_LIST_NUMBER (7)

typedef struct tagFrameBuf
{
    unsigned int size;
    unsigned int offset;
    unsigned int cap;
    char *buf;
}tFrameBuf;

typedef struct tagFrameList
{
    unsigned ulFrameNumber;
    unsigned ulCap;
    char     szFileName[128];
    tFrameBuf *ppstFrameBuf[M_FRAME_RATE];
}tFrameList;

typedef struct tagFrameLList
{
    unsigned int ulListNumber;
    unsigned ulCap;
    tFrameList *ppstList[M_LIST_NUMBER];
    pthread_mutex_t mutex;
}tFrameLList;

typedef struct tagAcClientData
{
//    tFrameBuf *pstFrameBuf;
    tFrameList *pstFrameList;
    tFrameLList stFreeFrameLList;
    tFrameLList stUsedFrameLList;
    tClientInfo stClient;    
}tAcClientData;

tFrameBuf *createFrameBuf(unsigned int cap)
{
    tFrameBuf *pstFrameBuf = (tFrameBuf *)calloc(1,sizeof(tFrameBuf));
    char *buf              =  (char *)calloc(cap,sizeof(char));
    
    if(NULL == pstFrameBuf || NULL == buf)
    {
        free(pstFrameBuf);
        free(buf);
        return NULL;
    }
    
    pstFrameBuf->size   = 0;
    pstFrameBuf->offset = 0;
    pstFrameBuf->cap    = cap;
    pstFrameBuf->buf    = buf;
    
    return pstFrameBuf;
}

tFrameList* creatList()
{
    unsigned index = 0;
    tFrameList *pstFrameList = (tFrameList *)calloc(1,sizeof(tFrameList));
    
    pstFrameList->ulFrameNumber = 0;
    pstFrameList->ulCap         = M_FRAME_RATE;
    
    for(index = 0;index < pstFrameList->ulCap; ++index)
    {
        pstFrameList->ppstFrameBuf[index] = createFrameBuf(M_MAX_FRAME_SIZE);
        debug("pstFrameList->ppstFrameBuf[index] %p\n",pstFrameList->ppstFrameBuf[index]);
    }
    
    return pstFrameList;
}

void initLList(tFrameLList *pstLList,unsigned cap,unsigned flag)
{
    unsigned index = 0;
    
    cap = cap < M_LIST_NUMBER ? cap : M_LIST_NUMBER;
    
    pthread_mutex_init(&pstLList->mutex,NULL);
    pstLList->ulListNumber = 0;
    pstLList->ulCap        = cap;
    
    if(flag)
    {
        for(index = 0;index < cap; ++index)
        {
            pstLList->ppstList[index] = creatList();
            debug("init List = %p\n",pstLList->ppstList[index]);
        }
        pstLList->ulListNumber = cap;
    }

    return ;
    
}

void addToLList(tFrameLList *pstFrameLList,tFrameList *pstFrameList)
{
    unsigned int ulListNumber;
    pthread_mutex_lock(&pstFrameLList->mutex);
    ulListNumber = pstFrameLList->ulListNumber;
    if(ulListNumber == M_LIST_NUMBER)
    {
        debug("full return ... \n");
        pthread_mutex_unlock(&pstFrameLList->mutex);
        return;
    }
    pstFrameLList->ppstList[ulListNumber] = pstFrameList;
    ++pstFrameLList->ulListNumber;
    
    pthread_mutex_unlock(&pstFrameLList->mutex);
    
    return;
}

void getFromLList(tFrameLList *pstFrameLList,tFrameList **ppstFrameList)
{
    unsigned int ulListNumber;
    pthread_mutex_lock(&pstFrameLList->mutex);
    ulListNumber = pstFrameLList->ulListNumber;
    if(ulListNumber == 0)
    {
        *ppstFrameList = NULL;
        //debug("empty return ... \n");
        pthread_mutex_unlock(&pstFrameLList->mutex);
        return;     
    }
    
    *ppstFrameList = pstFrameLList->ppstList[0];
    --pstFrameLList->ulListNumber;
    memmove(pstFrameLList->ppstList,&pstFrameLList->ppstList[1], pstFrameLList->ulListNumber * sizeof(tFrameList *));  
   
    pthread_mutex_unlock(&pstFrameLList->mutex);
    //debug("get List = %p\n",*ppstFrameList);
    return;
}

// getLocalTimeStr ignore error and null check
void getLocalTimeStr(char *str)
{
    struct timeval    tv; 
//    struct timezone tz;
    struct tm         *lt;
    gettimeofday(&tv, NULL); 
    
    lt = localtime(&tv.tv_sec);
    int ret = sprintf(str,"%04d%02d%02d-%02d%02d%02d.%ld.raw",
                      lt->tm_year+1900,lt->tm_mon+1,lt->tm_mday,
                      lt->tm_hour,lt->tm_min,lt->tm_sec,tv.tv_usec);
    str[ret] = 0;
    return;
}


int createTcpClient(char *ipaddr)
{
    int sockfd;
    int len;
    //struct sockaddr_un address;
    struct sockaddr_in address;
    int result;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ipaddr);
    address.sin_port = htons(SERVICE_PORT);
    len = sizeof(address);

    result = connect(sockfd, (struct sockaddr *)&address, len);

    if(result == -1) {
        debug("connect %s:%d error ,%s(%d)!\n",ipaddr,SERVICE_PORT,strerror(errno),errno);
        close(sockfd);
        sockfd = -1;
        //exit(1);
    }   
    return sockfd;
}

void saveFrameBuf2File(tFrameBuf *pstFrameBuf)
{
    char filename[128] = {0};
    getLocalTimeStr(filename);
    
    int savefd = open(filename,O_WRONLY | O_CREAT);
    
    if(savefd < 0)
    {
        debug("open %s error,sink error!\n",filename);
        return ;
    }
    
    int wr = write(savefd,pstFrameBuf->buf,pstFrameBuf->size);
    
    if(wr != pstFrameBuf->size)
    {
        debug("write error!\n");
        close(savefd);
        return;
    }
    fsync(savefd);
    close(savefd);
}

void sinkFrameList(tFrameList *pstFrameList)
{   
    unsigned int index = 0;
    static int savefd = -1;
    
    if(savefd < 0)
    {
        savefd = open(pstFrameList->szFileName,O_WRONLY | O_CREAT);
        if(savefd < 0)
        {
            debug("open %s error,sink error!\n",pstFrameList->szFileName);
            return ;
        }  
    }
    
    for(index = 0; index < pstFrameList->ulFrameNumber; ++index)
    {
        tFrameBuf *pstFrameBuf = pstFrameList->ppstFrameBuf[index];
        
        int wr = write(savefd,pstFrameBuf->buf,pstFrameBuf->size);
        
        if(wr != pstFrameBuf->size)
        {
            debug("write error!\n");
            close(savefd);
            savefd = -1;
            return;
        }
        
        pstFrameBuf->offset = 0;
        pstFrameBuf->size   = 0;
    }
    
    fsync(savefd);
 //   close(savefd);

    pstFrameList->szFileName[0] = 0;
    pstFrameList->ulFrameNumber = 0;

    return;
}

int recvOnePacket(int fd,char* buf,unsigned int length)
{
    int read = 0;
    unsigned int total = 0;
    while(1)
    {
        read = recv(fd, buf + total, length - total,0);
        if(read > 0)
        {
            total += read;
            if(total == length)
            {
                break;
            }
        }
        else if(read < 0 && ( errno == EINTR || errno == EAGAIN ))
        {
            debug("recvOnePacket read(%d) total(%d),error(%d:%s)!\n",read,total,errno,strerror(errno));
            continue;
        }
        else
        {
            debug("recvOnePacket read(%d) total(%d),error(%d:%s)!\n",read,total,errno,strerror(errno));
            break;
        }    
    }
    
    return total;
}

void clientRead(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
    char buffer[32] = {0};    
    tAcClientData *pstAcClientData = (tAcClientData *)watcher->data;
    if(pstAcClientData == NULL)
    {
        debug("client data error!");
        return;
    }
    tFrameList *pstFrameList = pstAcClientData->pstFrameList;
    tFrameBuf *pstFrameBuf = NULL;
    
    if(pstFrameList == NULL)
    {
        debug("free list number %u!\n",pstAcClientData->stFreeFrameLList.ulListNumber);
        getFromLList(&pstAcClientData->stFreeFrameLList,&pstAcClientData->pstFrameList); 
        if(NULL == pstAcClientData->pstFrameList)
        {
            debug("List NULL ,error !\n");
            return;
        }
        pstFrameList = pstAcClientData->pstFrameList;
        //debug("pstFrameList %p, number %u!\n",pstFrameList,pstFrameList->ulFrameNumber);
    }
    
    pstFrameBuf = pstFrameList->ppstFrameBuf[pstFrameList->ulFrameNumber];
    //debug("pstFrameBuf %p\n",pstFrameBuf);
    
    if(revents & EV_READ)
    {
        tNetMsg *pstNetMsg = (tNetMsg *)buffer;
        ssize_t read = recv(watcher->fd, buffer, sizeof(tNetMsg), 0);
        //debug("succeed type(%d)!\n",pstNetMsg->ulMsgType);
        if(read == sizeof(tNetMsg))
        {
            //read = recv(watcher->fd, pstFrameBuf->buf + pstFrameBuf->offset,pstNetMsg->ulMsgLen, 0);
            read = recvOnePacket(watcher->fd,pstFrameBuf->buf + pstFrameBuf->offset,pstNetMsg->ulMsgLen);
            if(read == pstNetMsg->ulMsgLen)
            {
                
                pstFrameBuf->offset += read;
                if(pstNetMsg->ulMsgType == M_FRAME_END)
                {
                    pstFrameBuf->size = pstFrameBuf->offset;
                    ++pstFrameList->ulFrameNumber;
                   
                }
                
                if(pstFrameList->ulFrameNumber == M_FRAME_RATE)
                {   
                    getLocalTimeStr(pstFrameList->szFileName);                    
                    addToLList(&pstAcClientData->stUsedFrameLList,pstFrameList);
                    debug("used list number %u!\n",pstAcClientData->stUsedFrameLList.ulListNumber);
                    pstAcClientData->pstFrameList = NULL;
                }
            }
            else
            {
                debug("packet broken recv(%d) length(%d),error(%d:%s)!\n",read,pstNetMsg->ulMsgLen,errno,strerror(errno));
            }
 
        }
        else if(read <= 0)
        {
            tClientInfo  *pstClient = &pstAcClientData->stClient;
            close(watcher->fd);
            ev_io_stop(loop, watcher);           
        }
        else
        {
            debug("read != sizeof(tNetMsg)\n");
        }
    }
    
    if(revents & EV_WRITE)
    {
        tNetMsg *pstNetMsg = (tNetMsg *)buffer;
        
        pstNetMsg->ulMsgType = M_START_RAW;
        pstNetMsg->ulMsgLen  = 0;
        if(send(watcher->fd,buffer,sizeof(tNetMsg),0) < 0)
        {
            debug("send M_START_RAW message error!\n");
        }
        else
        {
            ev_io_stop(loop, watcher);
            ev_io_init(watcher, clientRead, watcher->fd, EV_READ);
            ev_io_start(loop, watcher);
            debug("send start message ok!\n");
        }
    }
    
 
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

void sinkCallback (EV_P_ ev_timer *w, int revents)
{
	//debug("sink 1s frame ... \n");
    tAcClientData *pstAcClientData = (tAcClientData *)w->data;
    
    tFrameList *pstFrameList = NULL;
    debug("used list size %u\n",pstAcClientData->stUsedFrameLList.ulListNumber);
    getFromLList(&pstAcClientData->stUsedFrameLList,&pstFrameList); 
    
    if(pstFrameList)
    {
        
        sinkFrameList(pstFrameList);
        addToLList(&pstAcClientData->stFreeFrameLList,pstFrameList);
        debug("free list size %u\n",pstAcClientData->stFreeFrameLList.ulListNumber);
        
    }
    
    return;
}

void *threadSink(void *arg)
{
    struct ev_loop* loop = ev_loop_new(EVFLAG_AUTO);
    
    ev_timer watcher_timeout;
    watcher_timeout.data = arg;
    ev_timer_init (&watcher_timeout, sinkCallback, 0.0, 0.50);
	ev_timer_start (loop, &watcher_timeout);
    
    ev_run (loop, 0);

}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr,"usage:\n");
        fprintf(stderr,"    client [ipaddr]\n\n");
        return -1;
    }

    pthread_t threadID;
    struct ev_loop *loop = ev_default_loop(0);
    int sockfd = createTcpClient(argv[1]);
    struct ev_io evClient;
    struct sockaddr_in addr;
    int len;
    tAcClientData stAcClientData = {0};
        
    if(sockfd < 0)
    {
        debug("can not connected %s:%d!\n", argv[1], SERVICE_PORT);
        return -1;
    }
    
    initLList(&stAcClientData.stFreeFrameLList,M_LIST_NUMBER,1);
    initLList(&stAcClientData.stUsedFrameLList,M_LIST_NUMBER,0);
    
    getpeername(sockfd, (struct sockaddr *)&addr, &len);
    inet_ntop(AF_INET,&(addr.sin_addr),stAcClientData.stClient.ipaddr,sizeof(stAcClientData.stClient.ipaddr)); 
    stAcClientData.stClient.port = ntohs(addr.sin_port);
    
    debug("%s:%d fd(%d),connected!\n", stAcClientData.stClient.ipaddr, stAcClientData.stClient.port, sockfd);
    
    evClient.data = &stAcClientData;
    ev_io_init(&evClient, clientRead, sockfd, EV_READ | EV_WRITE);
    ev_io_start(loop, &evClient);

    ev_signal watcher_signalint;
	ev_signal_init(&watcher_signalint, signalCallback,SIGINT);
	ev_signal_start (loop, &watcher_signalint);
    
    ev_signal watcher_signalpipe;
	ev_signal_init(&watcher_signalpipe, signalCallback,SIGPIPE);
	ev_signal_start (loop, &watcher_signalpipe);
    
    pthread_create(&threadID,NULL,threadSink,&stAcClientData);
   
    ev_run (loop, 0);
    return 0;
    
}
