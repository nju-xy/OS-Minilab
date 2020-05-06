#include <common.h>
#include "devices.h"

//#define idle
//#define print_char_test
//#define producer_consumer_test
//#define echo_task_test

spinlock_t print_lk;
spinlock_t trap_lk;

#ifdef producer_consumer_test
sem_t fill;
sem_t empty;

void producer(void *arg) {
    while (1) {
        kmt->sem_wait(&empty);
        //kmt->spin_lock(&print_lk);
        printf("(");
        //kmt->spin_unlock(&print_lk);
        kmt->sem_signal(&fill);
    }
}

void consumer(void *arg) {
    while (1) {
        kmt->sem_wait(&fill);
        //kmt->spin_lock(&print_lk);
        printf(")");
        //kmt->spin_unlock(&print_lk);
        kmt->sem_signal(&empty);
    }
}
#endif

#ifdef print_char_test
void print_char(void *arg) {
    int t = (intptr_t)arg;
    while (1) {
        //printf("%d", t);
        printf("%c", "abcdefghijk"[t]);    
    }
}
#endif

#ifdef echo_task_test
void echo_task(void *name) {
    device_t *tty = dev_lookup(name);
    while (1) {
        char line[128], text[128];
        sprintf(text, "(%s) $ ", name); 
        // tty_write(tty, text);
        tty_write(tty, 0, (const void *)text, strlen(text));                    
        int nread = tty->ops->read(tty, 0, line, sizeof(line));
        line[nread - 1] = '\0';
        sprintf(text, "Echo: %s.\n", line); 
        // tty_write(tty, text);                    
        tty_write(tty, 0, (const void *)text, strlen(text));                    
    }
}
#endif

#ifdef idle
void idle_task() {
    while(1) {
        _yield();
    }
}
#endif

extern spinlock_t kmt_lk;
static void os_init() {
    kmt->spin_init(&print_lk, "print_lock");//永远在第一个
    kmt->spin_init(&trap_lk, "trap_lock");

#ifdef producer_consumer_test
    kmt->sem_init(&fill, "fill", 0);
    kmt->sem_init(&empty, "empty", 3);
#endif

    pmm->init();
    kmt->init();
    dev->init();
    vfs->init();
    
    // 可以去掉下面的所有task
#ifdef idle
    kmt->create(pmm->alloc(sizeof(task_t)), "idle1", idle_task, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "idle2", idle_task, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "idle3", idle_task, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "idle4", idle_task, NULL);
#endif

#ifdef echo_task_test
    kmt->create(pmm->alloc(sizeof(task_t)), "print1", echo_task, "tty1");
    kmt->create(pmm->alloc(sizeof(task_t)), "print2", echo_task, "tty2");
    kmt->create(pmm->alloc(sizeof(task_t)), "print3", echo_task, "tty3");
    kmt->create(pmm->alloc(sizeof(task_t)), "print4", echo_task, "tty4");
#endif

#ifdef producer_consumer_test
    kmt->create(pmm->alloc(sizeof(task_t)), "producer1", producer, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "producer2", producer, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "producer3", producer, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "consumer1", consumer, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "consumer2", consumer, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "consumer3", consumer, NULL);
#endif

#ifdef print_char_test
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-3", print_char, (void*)3);
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-2", print_char, (void*)2);
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-1", print_char, (void*)1);
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-0", print_char, (void*)0);
#endif
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-0", shell_thread, (void*)1);
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-1", shell_thread, (void*)2);
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-2", shell_thread, (void*)3);
    kmt->create(pmm->alloc(sizeof(task_t)), "test-thread-3", shell_thread, (void*)4);
    // Log("os_init");
}

static void hello() {
    kmt->spin_lock(&print_lk);
    for (const char *ptr = "Hello from CPU #"; *ptr; ptr++) {
        _putc(*ptr);
    }
    _putc("12345678"[_cpu()]); _putc('\n');
    kmt->spin_unlock(&print_lk);
}

/*
#define MAX_PTR_NR 1024
#define MAX_CPU 4
//void * test_mm[MAX_CPU][MAX_PTR_NR];
//void * space[100];

void *space[4][100];
void alloc_test() {
    //for (int i = 0; i < MAX_PTR_NR; i++) {
    //    int size = rand() % 1024 * 2;
    //    test_mm[_cpu()][i] = pmm->alloc(size);
    //    pmm->free(test_mm[_cpu()][i]);
    //}
    //for (int i = MAX_PTR_NR - 1; i >= 0; i--) {
    //for (int i = 0; i < MAX_PTR_NR; i++) {
    //    pmm->free(test_mm[_cpu()][i]);
    //}
    // the following part are changed from yzy's test.
    int i;
    for(i = 0; i < 100; ++i){
        space[_cpu()][i]=pmm->alloc(rand() & ((1 << 15) - 1));
    }
    for(i = 0;i < 1000;++i){
        int temp = rand() % 100;
        //pmm->free(space[_cpu()][temp]);
        space[_cpu()][temp] = pmm->alloc(rand() & ((1 << 15) - 1));
    }
    for(i = 0;i < 100; ++i){
        pmm->free(space[_cpu()][i]);
    }
    //int i;
    //for(i = 0; i < 100; ++i){
    //    printf("%d\n", i);
    //    space[i]=pmm->alloc(rand() & ((1 << 15) - 1));
    //}
    //for(i = 0;i < 1000;++i){
    //    printf("%d\n", i);
    //    int temp = rand() % 100;
    //    pmm->free(space[temp]);
    //    space[temp] = pmm->alloc(rand() & ((1 << 15) - 1));
    //}
    //for(i = 0;i < 100; ++i){
    //    printf("%d\n", i);
    //    pmm->free(space[i]);
    //}
    printf("End\n");
}
*/

#define VFS_TEST

#ifdef VFS_TEST
void vfs_test() {
    //int fd;
    //Log("access /a.c: %x\n", vfs->access("/a.c", 0));
    //fd = vfs->open("/a.c", FLAG_CREATE);
    /*char buf[20] = "lalala";
    char buf2[20] = "2333";
    char buf3[20] = {};

    vfs->write(fd, buf2, 4);
    vfs->write(fd, buf, 6);
    vfs->lseek(fd, -5, SEEK_END);
    vfs->write(fd, buf2, 3);
    vfs->lseek(fd, -1, SEEK_SET);
    vfs->read(fd, buf3, 20);
    
    kmt->spin_lock(&print_lk);
    Log("%s", buf3);
    kmt->spin_unlock(&print_lk);
    vfs->close(fd);
    vfs->link("/a.c", "/b.c");
    vfs->access("/b.c", 0);
    vfs->unlink("/b.c");
    vfs->access("/b.c", 0);
    vfs->unlink("/a.c");
    vfs->access("/a.c", 0);
    vfs->close(fd);
    vfs->access("/a.c", 0);
    vfs->access("/a.c", 0);
    vfs->open("/a.c", FLAG_CREATE);
    vfs->open("/a.c", FLAG_CREATE);
    vfs->mkdir("/Documents/xy");
    vfs->mkdir("/Documents");
    vfs->rmdir("/Documents");
    vfs->mkdir("/Documents");
    vfs->mkdir("/Documents/xy");
    fd = vfs->open("/Documents/xy/test2.c", FLAG_CREATE);
    vfs->open("/Documents/xy/test1.c", 0);
    vfs->close(fd);
    kmt->spin_lock(&print_lk);
    Log("cpu[%d] finish", _cpu());
    kmt->spin_unlock(&print_lk);*/
}
#endif

// 注: 这个死循环的进程我并没有保存下来
static void os_run() {
    hello();
    //alloc_test();
#ifdef VFS_TEST
    vfs_test(); 
#endif
    _intr_write(1);
    while (1) {
        _yield();
    }
}


struct my_irq_handler handlers[10];
int nr_handlers = 0;

static _Context *os_trap(_Event ev, _Context *context) {
    
    kmt->spin_lock(&trap_lk); 
    // Log("os trap");

    _Context *ret = NULL;
    for (int i = 0; i < nr_handlers; i++) {
        if (handlers[i].event == _EVENT_NULL || handlers[i].event == ev.event) {
            _Context *next = handlers[i].handler(ev, context);
            if (next) 
                ret = next;            
        }          
    }
    kmt->spin_unlock(&trap_lk);
    return ret;
}

static void os_on_irq(int seq, int event, handler_t handler) {
    // kmt->spin_lock(&print_lk);
    // Log("os_on_irq: seq:%d event:%d", seq, event);
    // kmt->spin_unlock(&print_lk);
    kmt->spin_lock(&trap_lk);
    int i = 0;
    for (i = 0; i < nr_handlers; i++) {
        if(seq <= handlers[i].seq) {
            break;
        }
    }
    for (int j = nr_handlers; j > i; j--) {
        handlers[j].seq = handlers[j - 1].seq;
        handlers[j].event = handlers[j - 1].event;
        handlers[j].handler = handlers[j - 1].handler;
    }
    handlers[i].seq = seq;
    handlers[i].event = event;
    handlers[i].handler = handler;
    nr_handlers++;
    if(nr_handlers >= 10) {
        kmt->spin_lock(&print_lk);
        Log("ERROR! Too much irq_handlers!");
        assert(0);
        kmt->spin_unlock(&print_lk);
    }
    kmt->spin_unlock(&trap_lk);
}

MODULE_DEF(os) {
  .init   = os_init,
  .run    = os_run,
  .trap   = os_trap,
  .on_irq = os_on_irq,
};
