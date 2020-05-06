#include "common.h"

extern spinlock_t print_lk;
extern task_t *Task[MAX_NR_TASKS];
task_t *cur_task[4] = {NULL, NULL, NULL, NULL};
// task_t *idle_task[4] = {NULL, NULL, NULL, NULL};// 用于保存最开始的os run进程, 用于没进程可以跑的时候跑
#define cur (cur_task[_cpu()])

_Context *kmt_context_save(_Event ev, _Context *ctx) {
    //TODO();     
    
#ifdef ASSERT_H
    kmt->spin_lock(&print_lk);
    if(cur && cur->fence1 != 0xcc) {
        Log("fence1 %d broken", cur->fence1);
        assert(0);
    }
    if(cur && cur->fence2 != 0xcc) {
        Log("fence2 %d broken", cur->fence2);
        assert(0);
    }
    assert(cur->status != TASK_READY);
    kmt->spin_unlock(&print_lk);
#endif

    if(cur)
        cur->context = *ctx;
    if(cur && cur->suicide) {
        Task[cur->id] = NULL;
        cur = NULL;
    }
    if(cur && cur->status == TASK_RUNNING) {
#ifdef ASSERT_H
        kmt->spin_lock(&print_lk);
        //Log("CPU[%d] changing task %s's status from %d to %d", _cpu(), cur->name, cur->status, TASK_READY);
        kmt->spin_unlock(&print_lk);
#endif
        
        cur->status = TASK_READY;
    }  
    return ctx;    
}

_Context *kmt_context_switch(_Event ev, _Context *ctx) {
    // TODO();
    // Log("kmt_context_switch"); 
    
#ifdef ASSERT_H
    kmt->spin_lock(&print_lk);
    if(cur && cur->fence1 != 0xcc) {
        Log("fence1 %d broken", cur->fence1);
        assert(0);
    }
    if(cur && cur->fence2 != 0xcc) {
        Log("fence2 %d broken", cur->fence2);
        assert(0);
    }
    kmt->spin_unlock(&print_lk);
#endif


    task_t *ret = cur;
    if(!cur) {
        //kmt->spin_lock(&print_lk);
        //Log("CPU[%d]: Schedule first time", _cpu());
        //kmt->spin_unlock(&print_lk);
        ret = Task[0];
    }
    
    for (int i = ret->id + 1; i <= MAX_NR_TASKS + ret->id + 1; i++) {
        task_t *tsk = Task[i % MAX_NR_TASKS];
        if(tsk && tsk->id % _ncpu() == _cpu() && tsk->status == TASK_READY) {
            ret = tsk; 
            break;
        }
    }
    
    //kmt->spin_lock(&print_lk);
    //Log("CPU[%d]: Schedule from Task %s to Task %s", _cpu(), cur->name, ret->name);
    //kmt->spin_unlock(&print_lk);
    
    cur = ret;
    
#ifdef ASSERT_H
    kmt->spin_lock(&print_lk);
    if(cur && cur->status != TASK_READY && cur->status != TASK_WAKE_UP) {
        Log("CPU[%d]: Task[%d] %s is %d while switching", _cpu(), cur->id, cur->name, cur->status);
        assert(0);
    }

    for (int i = 0; i < 4; i++) {
        if(cur_task[i] && i != _cpu() && cur_task[i]->id == cur->id) {
            Log("Task %s is running on CPU[%d], cannot run on CPU[%d]", ret->name, i, _cpu());
        assert(0);
        }
    }
    //Log("CPU[%d] changing task %s's status from %d to %d",_cpu(), cur->name, cur->status, TASK_RUNNING);
    kmt->spin_unlock(&print_lk);
#endif

    cur->status = TASK_RUNNING;
   
#ifdef ASSERT_H
    kmt->spin_lock(&print_lk);
    assert(cur);
    if(cur && cur->status != TASK_RUNNING) {
        Log("CPU[%d]: Task[%d] %s is %d after switching", _cpu(), cur->id, cur->name, cur->status);
        assert(0);
    }
    if(cur && cur->fence1 != 0xcc) {
        Log("fence1 %d broken", cur->fence1);
        assert(0);
    }
    if(cur && cur->fence2 != 0xcc) {
        Log("fence2 %d broken", cur->fence2);
        assert(0);
    }
    kmt->spin_unlock(&print_lk);
#endif
    
    return &cur->context;
}
