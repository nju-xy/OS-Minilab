#include "common.h"

extern spinlock_t print_lk;
extern spinlock_t trap_lk;
task_t *Task[MAX_NR_TASKS] = {}; 

extern void spin_init (spinlock_t *lk, const char *name);
extern void spin_lock (spinlock_t *lk);
extern void spin_unlock (spinlock_t *lk);
extern void sem_init(sem_t *sem, const char *name, int value);
extern void sem_wait(sem_t *sem);
extern void sem_signal(sem_t *sem);

void kmt_idle_task() {
    //while(1) {
    //    _yield();
    //}
    while(1);
}

void init() {
    _cte_init(os->trap);
    os->on_irq(INT_MIN, _EVENT_NULL, kmt_context_save); 
    // 总是最先调用
    os->on_irq(INT_MAX, _EVENT_NULL, kmt_context_switch); 
    // 总是最后调用

    // 下面四个伪Task(就是不断yield) 用于防止没东西能调度
    // 等价于保存那些os run的初始进程, 不过运用了现成的函数, 方便
    kmt->create(pmm->alloc(sizeof(task_t)), "kmt_idle1", kmt_idle_task, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "kmt_idle2", kmt_idle_task, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "kmt_idle3", kmt_idle_task, NULL);
    kmt->create(pmm->alloc(sizeof(task_t)), "kmt_idle4", kmt_idle_task, NULL);
}

int create(task_t *task, const char *name, void (*entry)(void *arg), void *arg) {
    spin_lock(&trap_lk);
    //Log("CPU[%d] create task %s", _cpu(), name);
    task->name = name;
    task->suicide = 0;
    // strncpy(task->name, name, 20);
    /*
    _Area stk;
    stk.start = (void *)&task->context;
    stk.end = (void *)&task->stack[STK_SZ];
    _kcontext(stk, entry, arg);
    */
    _Area stk = (_Area){task->stack, task->stack + STK_SZ}; 
    task->context = *_kcontext(stk, entry, arg);
    

    int create_flag = 0; 
    for (int i = 0; i < MAX_NR_TASKS; i++) {
        if(Task[i] == NULL) {
            task->id = i;
            Task[i] = task;
            create_flag = 1;
            break;
        }
    }
    if(create_flag == 0) {
        spin_lock(&print_lk);
        Log("Too much Tasks, no free ones");
        assert(0);
        spin_unlock(&print_lk);
    }
    // Log("thread id: %d", Task[task->id]->id);
    task->status = TASK_READY;
    task->fence1 = task->fence2 = 0xcc;
    spin_unlock(&trap_lk);
    return task->id;
}

void teardown(task_t *task) {
    spin_lock(&trap_lk);
    // not tested!!! not free memory indeed
    // Log("tear down task %s", task->name);
    
    task->suicide = 1;
    
    //pmm->free((void*)task);
    spin_unlock(&trap_lk);
}


MODULE_DEF(kmt) {
    .init = init,
    .create = create,
    .teardown = teardown,
    .spin_init = spin_init,
    .spin_lock = spin_lock,
    .spin_unlock = spin_unlock,
    .sem_init = sem_init,
    .sem_wait = sem_wait,
    .sem_signal = sem_signal,
};
