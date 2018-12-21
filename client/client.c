#include "ev.h"
#include "netcommon.h"
#include <pthread.h>

#define M_FRAME_RATE (25)
#define M_LIST_NUMBER (20)
#define M_DEFAULT_FRAME_BUF_SIZE (20*1024*1024)
#define M_DEFAULT_FRAME_BUF_CALL (M_DEFAULT_FRAME_BUF_SIZE + 4*1024*1024)

typedef struct tagFrameBuf
{
    unsigned int size;
    unsigned int offset;
    unsigned int cap;
    char *buf;
}tFrameBuf;

typedef struct tagFrameNode tFrameNode;
struct tagFrameNode
{
    tFrameBuf *pstFrameBuf;
    tFrameNode *pstNext;
};

typedef struct tagFrameList tFrameList;
struct tagFrameList
{
    pthread_mutex_t mutex;
    tFrameNode *pstHead;
    tFrameNode *pstTail;
};

typedef struct tagAcClientData
{
    tFrameNode *pstFrameNode;
    tFrameList stFrameFreeList;
    tFrameList stFrameUsedList;
    tClientInfo stClient;
    char     szFileName[128];
    int fd;
}tAcClientData;

int addtoFrameList(tFrameList *pstList,tFrameNode *pstNode);
int createFrameList(tFrameList *pstFrameList)
{
    unsigned int index = 0;

    if(NULL == pstFrameList)
    {
        debug("calloc pstFrameList error \n");
        return -1;
    }
    
    for(index = 0 ;index < M_LIST_NUMBER; ++index)
    {
        tFrameNode *pstNode = (tFrameNode *)calloc(1,sizeof(tFrameNode));
        tFrameBuf *pstBuf = (tFrameBuf *)calloc(1,sizeof(tFrameBuf));
        char *buf = (char *)calloc(1,M_DEFAULT_FRAME_BUF_CALL);
        if(NULL == pstNode || NULL == pstBuf || NULL == buf)
        {
            debug("create node error!\n");
            free(pstNode);
            free(pstBuf);
            free(buf);
            return -1;
        }
        pstBuf->size = 0;
        pstBuf->offset = 0;
        pstBuf->cap = M_DEFAULT_FRAME_BUF_SIZE;
        pstBuf->buf = buf;
        pstNode->pstFrameBuf = pstBuf;
        pstNode->pstNext = NULL;
        addtoFrameList(pstFrameList,pstNode);  
    }   
    return 0;
}

int addtoFrameList(tFrameList *pstList,tFrameNode *pstNode)
{
    if(NULL == pstList || NULL == pstNode)
    {
        debug("pstList = %p,pstNode = %p\n",pstList,pstNode);
        return -1;
    }

    pthread_mutex_lock(&pstList->mutex);

    pstNode->pstNext = NULL;
    if( NULL != pstList->pstTail)
    {
        pstList->pstTail->pstNext = pstNode;                
    }
    else
    {
        pstList->pstHead = pstNode;
    }
    pstList->pstTail = pstNode;
    pthread_mutex_unlock(&pstList->mutex);
    return 0;
}

int getFromFrameList(tFrameList *pstList,tFrameNode **ppstNode)
{
    if(NULL == pstList || NULL == ppstNode)
    {
        debug("pstList = %p,ppstNode = %p\n",pstList,ppstNode);
        return -1;
    }
    pthread_mutex_lock(&pstList->mutex);
    *ppstNode = pstList->pstHead;
    
    if(pstList->pstHead)
    {        
        pstList->pstHead = pstList->pstHead->pstNext;  
        if(pstList->pstHead == NULL)
        {
            pstList->pstTail = NULL;
        }
    }
    pthread_mutex_unlock(&pstList->mutex);
    return 0;
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
    tAcClientData *pstAcClientData = (tAcClientData *)watcher->data;
    if(pstAcClientData == NULL)
    {
        debug("client data error!");
        return;
    }
    
    tFrameBuf *pstFrameBuf = NULL;
    if(pstAcClientData->pstFrameNode == NULL)
    {
        //debug("get from free list\n");
        getFromFrameList(&pstAcClientData->stFrameFreeList, &pstAcClientData->pstFrameNode);
    }
    
    if(pstAcClientData->pstFrameNode)
    {
        pstFrameBuf = pstAcClientData->pstFrameNode->pstFrameBuf;
    }
    
    if(NULL == pstFrameBuf)
    {
        debug("buf error!\n");
        return;
    }
    
    if(revents & EV_READ)
    {
        ssize_t read = recv(watcher->fd, pstFrameBuf->buf + pstFrameBuf->offset, M_PACKET_SIZE, 0);
        
        //if(pstFrameBuf->offset%(1500*100) == 0)
        //    debug("pstFrameBuf->offset = %u,pstFrameBuf->cap = %u\n",pstFrameBuf->offset,pstFrameBuf->cap);
        if(read > 0)
        {
            pstFrameBuf->offset += read;
        }
        else if(read < 0 && ( errno == EINTR || errno == EAGAIN ))
        {
            debug("read(%d) ,error(%d:%s)!\n",read,errno,strerror(errno));
            return;
        }
        else if(read <= 0)
        {
            tClientInfo  *pstClient = &pstAcClientData->stClient;
            close(watcher->fd);
            ev_io_stop(loop, watcher); 
        }
        
        if(pstFrameBuf->offset >= pstFrameBuf->cap)
        {
            //debug("add to used list\n");
            pstFrameBuf->size = pstFrameBuf->offset;
            addtoFrameList(&pstAcClientData->stFrameUsedList,pstAcClientData->pstFrameNode);
            pstAcClientData->pstFrameNode = NULL;
        }
        
    }
    
    if(revents & EV_WRITE)
    {
        char buffer[32] = {0};
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

void sinkFrameNode(int fd,tFrameNode *pstFrameNode)
{
    if(NULL == pstFrameNode || NULL == pstFrameNode->pstFrameBuf)
        return;
    
    tFrameBuf *pstFrameBuf = pstFrameNode->pstFrameBuf;
    
    if(write(fd,pstFrameBuf->buf,pstFrameBuf->size) != pstFrameBuf->size)
    {
        debug("write error!\n");
        pstFrameBuf->size = 0;
        pstFrameBuf->offset = 0;
        return;
    }
    fsync(fd);
    
    pstFrameBuf->size = 0;
    pstFrameBuf->offset = 0;
    return;
}

void sinkCallback (EV_P_ ev_timer *w, int revents)
{
	//debug("sink 1s frame ... \n");
    tAcClientData *pstAcClientData = (tAcClientData *)w->data;
    tFrameNode *pstFrameNode = NULL;
    if(pstAcClientData->fd == 0)
    {
        getLocalTimeStr(pstAcClientData->szFileName);  
        pstAcClientData->fd = open(pstAcClientData->szFileName,O_WRONLY | O_CREAT);
        
        if(pstAcClientData->fd < 0)
        {
            debug("create sink file error!\n");
            return;
        }       
    }
    
    
    getFromFrameList(&pstAcClientData->stFrameUsedList,&pstFrameNode);
    
    if(pstFrameNode)
    {
        //debug("sink one node\n");
        sinkFrameNode(pstAcClientData->fd,pstFrameNode);
        addtoFrameList(&pstAcClientData->stFrameFreeList,pstFrameNode);
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
    
    pthread_mutex_init(&stAcClientData.stFrameFreeList.mutex,NULL);
    pthread_mutex_init(&stAcClientData.stFrameUsedList.mutex,NULL);
    debug("\n");
    createFrameList(&stAcClientData.stFrameFreeList);
    debug("\n");

    debug("\n");
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
