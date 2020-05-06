#ifndef __COMMON_H__
#define __COMMON_H__

//#define ASSERT_H

#include <kernel.h>
#include <nanos.h>
#include "vfs.h"
#include "klib.h"
#include "am.h"
#include "debug.h"


#define INT_MAX 2147483647
#define INT_MIN (-INT_MAX - 1)

#define true 1
#define false 0

#define MAX(a, b) (a < b ? b : a)
#define MIN(a, b) (a > b ? b : a)

#define LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))

#define STK_SZ 4096
#define MAX_NR_TASKS 25

#define TASK_WAITING 2
#define TASK_RUNNING 1
#define TASK_READY 0
#define NR_FILE 256
void shell_thread(void * tty_id);
int find_mount_point(filesystem_t *fs, const char *path);

struct task {
    int id;
    // int cpu;
    const char *name;
    char pwd[64];
    int status;
    int suicide;
    _Context context;
    int fence1;
    uint8_t stack[STK_SZ];
    int fence2;
    file_t *fildes[NR_FILE];
};

struct spinlock {
    int locked;
    int cpu;
    int magic;
    const char *name;
};

#define MAX_IN_QUE MAX_NR_TASKS + 5
struct semaphore {
    const char *name;
    int value;
    spinlock_t lock;
    task_t * que[MAX_IN_QUE];
    int nr_wait;
};

_Context *kmt_context_save(_Event ev, _Context *ctx);
_Context *kmt_context_switch(_Event ev, _Context *ctx);

struct my_irq_handler {
    int event;
    int seq;
    _Context *(*handler)(_Event ev, _Context *ctx);
};

#include "devices.h"
#endif
