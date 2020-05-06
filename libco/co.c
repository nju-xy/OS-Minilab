#include <stdio.h>
#include <stdlib.h>
//#include <setjmp.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include "co.h"

#define MAX_CO 10

struct co {
    uint8_t stack[4096];
    ucontext_t context;
    //jmp_buf buf;
    func_t func;
    void* arg;
    char name[100];
    int id, wait, wait_by;
    bool death, occupied;
}__attribute__ ((aligned (16))); 

struct co coroutines[MAX_CO]; //静态分配
int cur; //当前协程号
int no = 1;

//初始化
void co_init() {
    no = 1;
    srand(time(0));
    co_start("main", NULL, NULL);
    getcontext(&coroutines[1].context);
    cur = 1;
}

void my_func(int n) {
    coroutines[n].func(coroutines[n].arg);
    //printf("\ncoroutine %d die.\n", n);
    coroutines[n].death = 1;
    coroutines[n].occupied = 0;
    no --;
    if(coroutines[n].wait_by) {
        cur = coroutines[n].wait_by;
        swapcontext(&coroutines[n].context, &coroutines[cur].context);
    }
    else {
        int r = rand() % MAX_CO;
        while (!coroutines[r].occupied || coroutines[r].death || coroutines[r].wait) {// || (coroutines[r].id == cur)) {
            r = rand() % MAX_CO;
        }
        int ori_cur = cur;
        cur = r;
        swapcontext(&coroutines[ori_cur].context, &coroutines[r].context);
    } 
}

//创建一个新的协程,并返回一个指针(动态分配内存)
struct co* co_start(const char *name, func_t func, void *arg) {
    no++;
    assert(no < MAX_CO);
    int k = 1;
    for(k = 1; k < MAX_CO; k++) {
        if(!coroutines[k].occupied)
            break;
    }
    coroutines[k].id = k;
    coroutines[k].func = func;
    coroutines[k].arg = arg;
    coroutines[k].wait = 0;
    coroutines[k].wait_by = 0;
    coroutines[k].death = 0;
    coroutines[k].occupied = 1;
    memset(coroutines[k].stack, 0, sizeof(coroutines[k].stack));
    strncpy(coroutines[k].name, name, 99);
    struct co * ptr = &coroutines[k];
    
    getcontext(&coroutines[k].context);
    coroutines[k].context.uc_stack.ss_sp = coroutines[k].stack;
    coroutines[k].context.uc_stack.ss_size = sizeof(coroutines[k].stack);
    coroutines[k].context.uc_stack.ss_flags = 0;
    //makecontext(&coroutines[no].context, (void (*) (void))func, 1, arg);
    makecontext(&coroutines[k].context, (void (*) (void))my_func, 1, k);

    printf("start %d %s\n", k, name);
    
    //func(arg); // Test #2 hangs
    return ptr;
}

/*
当前运行的协程放弃执行, 并切换到其他协程执行
随机选择下一个系统中可运行的协程
*/
void co_yield() {
    /*int r = rand() % (no - 1) + 1;
    while (coroutines[r].death || coroutines[r].wait) {// || (coroutines[r].id == cur)) {
        r = rand() % (no - 1) + 1;
    }*/
    int r = rand() % MAX_CO;
    while (!r || !coroutines[r].occupied || coroutines[r].death || coroutines[r].wait) {// || (coroutines[r].id == cur)) {
        r = rand() % MAX_CO;
    }
    //printf("yield from %d to %d %d\n", cur, r, coroutines[r].id);
    int ori_cur = cur;
    cur = r;
    swapcontext(&coroutines[ori_cur].context, &coroutines[r].context);
    
    /*
    int val = setjmp(coroutines[cur].buf);
    if (val == 0) {
        // 立即返回
        // 随机切换进程
        int r = rand() % (no - 1) + 1;
        while (coroutines[r].death || coroutines[r].wait || (coroutines[r].id == cur)) {
            r = rand() % (no - 1) + 1;
        }
        //printf("yield from %d to %d %d\n", cur, r, coroutines[r].id);
        cur = r;
        if(coroutines[r].on) {
            longjmp(coroutines[r].buf, cur);
        }
        else {
            coroutines[r].on = 1;
            coroutines[r].func(coroutines[r].arg);
        }
    } else {
        // 由 longjmp 返回
        //printf("ret from longjmp in %d to %d", val, cur);
    }
    */
}

/* 
当前协程不再执行, 直到 thd 协程的执行完成。
我们规定每个协程的资源在 co_wait() 等待结束后释放,
每个协程必须恰好被 co_wait 一次
*/
void co_wait(struct co *thd) {
    //printf("co_wait %d\n", thd->id);
    if(thd->death) return;
    coroutines[cur].wait = thd->id;
    thd->wait_by = cur;
    int ori_cur = cur;
    cur = thd->id;
    //thd->context.uc_link = &(coroutines[ori_cur].context);
    //makecontext(&thd->context, (void (*) (void))thd->func, 1, thd->arg);
    swapcontext(&(coroutines[ori_cur].context), &(thd->context)); 
         
    /*
    int val = setjmp(coroutines[cur].buf);
    if(val == 0) {
        cur = thd->id;
        if(thd->on)
            longjmp(thd->buf, thd->wait_by);
        else {
            thd->on = 1;
            thd->func(thd->arg);
        }
    }
    */
}

