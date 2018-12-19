#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define M_MAX_COUNT (25)
#define M_BUF_SIZE (4*1024*1024)

void getLocalTimeStr(char *str)
{
    struct timeval    tv; 
//    struct timezone tz;
    struct tm         *lt;
    gettimeofday(&tv, NULL); 
    
    lt = localtime(&tv.tv_sec);
    sprintf(str,"%04d%02d%02d-%02d%02d%02d.%ld.raw",
            lt->tm_year+1900,lt->tm_mon+1,lt->tm_mday,
            lt->tm_hour,lt->tm_min,lt->tm_sec,tv.tv_usec);
    return;
}

int main()
{
    char filename[128] = {0};
    int index = 0;    
    char *buf = (char *)malloc(M_BUF_SIZE);
    struct timespec starttime ;
    struct timespec endtime ;
    
    getLocalTimeStr(filename);
    
    clock_gettime(CLOCK_MONOTONIC, &starttime);
    int savefd = open(filename,O_WRONLY | O_CREAT);
    
    if(savefd < 0)
    {
        fprintf(stderr,"open %s error,sink error!\n",filename);
        return ;
    }
        
    for(index = 0 ; index < M_MAX_COUNT; ++index)
    {
        int wr = write(savefd,buf,M_BUF_SIZE);
        
        if(wr != M_BUF_SIZE)
        {
            fprintf(stderr,"write error!\n");
            close(savefd);
            return;
        }
    }

    fsync(savefd);
    close(savefd);
    clock_gettime(CLOCK_MONOTONIC, &endtime);
    
    unsigned long long start = starttime.tv_sec * 1000 + starttime.tv_nsec/(1000*1000);
    unsigned long long end   = endtime.tv_sec * 1000 + endtime.tv_nsec/(1000*1000);
    unsigned long long diff  = end - start;
    
    unsigned long long total = M_BUF_SIZE;
    total *= M_MAX_COUNT;
    
    double speed = (total*1.0)/(diff*1024*1024);
    speed *= 1000;
    
    fprintf(stderr,"start ts(%llu ms), end ts(%llu ms) ,diff (%llu ms)\n",
            start,
            end,
            diff
           );
    fprintf(stderr,"total (%llu MB),speed %f MB/s\n",total/(1024*1024),speed);

    return 0;
}
