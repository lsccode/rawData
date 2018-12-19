#ifndef __LOG_DEBUG_H__
#define __LOG_DEBUG_H__
#include <stdio.h>

#define debug(format,...) \
    do{ \
    fprintf(stderr,"file( %s ), fun( %s ),line( %d ), "format, __FILE__,__func__,__LINE__, ##__VA_ARGS__); \
} \
while(0)

#endif
