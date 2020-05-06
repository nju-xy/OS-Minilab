#ifndef __DEBUG_H__
#define __DEBUG_H__

#define Log(format, ...) \
    printf("\33[1;35m[%s,%d,%s] " format "\33[0m\n", \
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#define TODO() Log("Please implement me."); assert(0);
#endif
