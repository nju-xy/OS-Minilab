#include "common.h"

extern int ncli[4];
spinlock_t kmt_lk;
extern spinlock_t print_lk;
extern void spin_init (spinlock_t *lk, const char *name);
extern void spin_lock (spinlock_t *lk);
extern void spin_unlock (spinlock_t *lk);

void sem_init(sem_t *sem, const char *name, int value) {
    // strncpy(sem->name, name, 20);
    sem->name = name;
    sem->value = value;
    sem->nr_wait = 0;
    // kmt->spin_lock(&print_lk);
    // Log("sem %s init, value = %d", sem->name, sem->value);
    // kmt->spin_unlock(&print_lk);
    spin_init(&sem->lock, sem->name);
    // cond_init(&s->cond);
    // TODO();
}

extern task_t *cur_task[4];
#define cur (cur_task[_cpu()])

void sem_wait(sem_t *sem) {
    spin_lock(&sem->lock);
    /*
    kmt->spin_lock(&print_lk);
    if(cur->status != TASK_RUNNING) {
        Log("CPU[%d]: TASK %s status %d, sem %s", _cpu(), cur->name, cur->status, sem->name);
        assert(0);
    }
    kmt->spin_unlock(&print_lk);
    */

#ifdef ASSERT_H
    for (int i = 0; i < sem->nr_wait; i++) {
        if(sem->que[i] == cur) {
            Log("twice in que");
            kmt->spin_lock(&print_lk);
            assert(0);
            kmt->spin_unlock(&print_lk);
        }
    }
#endif
    
    // kmt->spin_lock(&print_lk);
    // Log("CPU[%d]: %s sem wait %s, val is %d, %d in que", _cpu(), cur->name, sem->name, sem->value, sem->nr_wait);
    // kmt->spin_unlock(&print_lk);
    while(sem->value <= 0) {
        // cond_wait(&sem->cond, &sem->lock);

#ifdef ASSERT_H
        kmt->spin_lock(&print_lk);
        assert(sem->value == 0); 
        // Log("CPU[%d]: Task %s sem wait: %s, sem value = %d, nr_wait = %d", _cpu(), cur->name, sem->name, sem->value, sem->nr_wait);
        kmt->spin_unlock(&print_lk);
#endif

        for (int i = sem->nr_wait; i > 0; i--) {
            sem->que[i] = sem->que[i - 1];
        }
        sem->que[0] = cur;
        sem->nr_wait ++;
            
        kmt->spin_lock(&print_lk);
        if(sem->nr_wait >= MAX_IN_QUE - 2) {
            Log("Too much(%d) in sem queue!", sem->nr_wait);
            assert(0);
        }
        kmt->spin_unlock(&print_lk);

        //kmt->spin_lock(&print_lk);
        //Log("CPU[%d] changing task %s's status from %d to %d", _cpu(), cur->name, cur->status, TASK_WAITING);
        //kmt->spin_unlock(&print_lk);
        cur->status = TASK_WAITING;
        
        spin_unlock(&sem->lock);

        _yield();        
        
        spin_lock(&sem->lock);
    }

    sem->value --;
    
    // kmt->spin_lock(&print_lk);
    // Log("CPU[%d] %s wake up after receiving %s", _cpu(), cur->name, sem->name);
    // kmt->spin_unlock(&print_lk);
    
    spin_unlock(&sem->lock);
}

void sem_signal(sem_t *sem) {
    spin_lock(&sem->lock);
    // Log("sem_signal: %s", sem->name);
    sem->value ++;
    // Log("sem %s value now = %d", sem->name, sem->value);
    if (sem->nr_wait > 0) {       
        task_t * task_to_wake = sem->que[sem->nr_wait - 1];
        sem->nr_wait--;
        task_to_wake->status = TASK_READY;
#ifdef ASSERT_H
        kmt->spin_lock(&print_lk);
        assert(sem->nr_wait >= 0);
        assert(sem->value > 0);
        // Log("CPU[%d]: %s signal %s, %d in que left", _cpu(), sem->name, task_to_wake->name, sem->nr_wait);
        kmt->spin_unlock(&print_lk);
#endif
    }
    spin_unlock(&sem->lock);
}
