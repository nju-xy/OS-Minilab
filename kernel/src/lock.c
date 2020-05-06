#include "common.h"
#include "x86.h"

int ncli[4] = {};
int intena[4] = {}; // 1则在不在中断处理程序中, 0则在

void pushcli(void) {
    int eflags = get_efl();
    cli();
    if(ncli[_cpu()] == 0)
        intena[_cpu()] = eflags & FL_IF;
    ncli[_cpu()] ++;
    //printf("cpu #%d push cli\n", _cpu());
}

void popcli(void) {
    if(get_efl()&FL_IF) {
        Log("popcli - interruptible");
        assert(0);
    }
    //printf("cpu #%d pop cli\n", _cpu());
    if(--ncli[_cpu()] < 0) {
        Log("cpu #%d pop too much cli\n", _cpu());
        assert(0);
    }
    if(ncli[_cpu()] == 0 && intena[_cpu()])
        sti();
}

int holding(spinlock_t *lk) {
    int r;
    pushcli();
    r = lk->locked && lk->cpu == _cpu();
    popcli();
    return r;
}

void spin_init (spinlock_t *lk, const char *name){
    lk->locked = 0;
    lk->magic = 0xdd;
    // strncpy(lk->name, name, 20);
    lk->name = name;
    lk->cpu = -1;
    //Log("lock %s init, magic %d, cpu %d, locked %d, addr 0x%x", lk->name, lk->magic, lk->cpu, lk->locked, (int)lk);
    assert(lk->magic == 0xdd);
}

void spin_lock (spinlock_t *lk) {
    //Log("\nCPU[%d] lock %s, addr %x", _cpu(), lk->name, (int)lk);
    if(lk->magic != 0xdd)
        Log("lock %s, magic %d, cpu %d, locked %d, addr 0x%x", lk->name, lk->magic, lk->cpu, lk->locked, (int)lk);
    assert(lk->magic == 0xdd);
    pushcli();
    if(holding(lk)) {
        Log("\nSpin_lock: CPU[%d] hold the lock, addr 0x%x, name %s\n", _cpu(), (int)lk, lk->name);
        assert(0);
    }
    while (_atomic_xchg(&lk->locked, 1)) ;
    __sync_synchronize();
    lk->cpu = _cpu();
}

void spin_unlock (spinlock_t *lk) {
    //Log("\nCPU[%d] unlock %s, addr %x", _cpu(), lk->name, (int)lk);
    if(lk->magic != 0xdd)
        Log("\nlock %s, magic %d, cpu %d, locked %d, addr 0x%x", lk->name, lk->magic, lk->cpu, lk->locked, (int)lk);
    assert(lk->magic == 0xdd);
    if(!holding(lk)) {
        Log("\nSpin_unlock: CPU[%d] don't hold the lock, addr 0x%x, name %s\n", _cpu(), (int)lk, lk->name);
        assert(0);
    }
    lk->cpu = -1;
    __sync_synchronize();
    _atomic_xchg(&lk->locked, 0);
    popcli();
}
