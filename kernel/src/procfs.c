#include "common.h"
int proc_lookup(const char * path) {
    char temp[5] = {};
    for (int i = 0; i < MAX_NR_TASKS; i++) {
        sprintf(temp, "%d", i);
        if(strncmp(path, temp, 32) == 0) {
            return 1;
        }
    }
    if(strncmp(path, "meminfo", 32) == 0) {
        return 1;
    }
    if(strncmp(path, "cpuinfo", 32) == 0) {
        return 1;
    }
    return 0;
}

extern uintptr_t pm_start, pm_end, pm_used;

extern task_t *Task[MAX_NR_TASKS];
int proc_read(const char * path, void * buf, size_t nbytes) {
    char temp[5] = {};
    char * proc_buf = pmm->alloc(128);
    for (int i = 0; i < MAX_NR_TASKS; i++) {
        sprintf(temp, "%d", i);
        if(strncmp(path, temp, 32) == 0) {
            char *status[3] = {"waiting", "running", "ready"};
            if(Task[i]) 
                sprintf(proc_buf, "Task id: %d\nTask name: %s\nTask status: %s\n", Task[i]->id, Task[i]->name, status[Task[i]->status]);
            else
                sprintf(proc_buf, "Task %d doesn't exist\n", i);
            strncpy(buf, proc_buf, nbytes);
            pmm->free(proc_buf);
            return MIN(nbytes, sizeof(proc_buf));
        }
    }
    if(strncmp(path, "meminfo", 32) == 0) {
        sprintf(proc_buf, "Total: %d\npm_start: %d\npm_end: %d\nused memory: %d\nfree memory: %d\n", (int)pm_end - (int)pm_start, (int)pm_start, (int)pm_end, (int)pm_used, (int)pm_end - (int)pm_start - (int)pm_used);
        strncpy(buf, proc_buf, nbytes);
        pmm->free(proc_buf);
        return MIN(nbytes, sizeof(proc_buf));
    }
    if(strncmp(path, "cpuinfo", 32) == 0) {
        sprintf(proc_buf, "Total number of cpu: %d\n", _ncpu());
        strncpy(buf, proc_buf, nbytes);
        pmm->free(proc_buf);
        return MIN(nbytes, sizeof(proc_buf));
    }
    return 0;
}
