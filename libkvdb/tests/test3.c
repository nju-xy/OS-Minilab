// 多线程多进程测试
#include "kvdb.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
 
#define NUM_THREADS 8

void my_panic(char* str, int err) {
    if(err)
        printf("%s: %d\n", str, err);
    else
        printf("%s\n", str);
    assert(0);
}

kvdb_t db;
void * thread_test(void* arg) {
    int no = *(int*)arg;
    const char *key = "operating-systems";
    char *value;
    
    int err;
    err = kvdb_open(&db, "a.db");
    if(err)
        my_panic("kvdb open failed", err); 
    
    err = kvdb_put(&db, key, "three-easy-pieces");
    if(err)
        my_panic("kvdb put failed", err); 
    
    value = kvdb_get(&db, key);
    if(!value)
        my_panic("kvdb get failed", 0);
    printf("Thread %d: [%s]: [%s]\n", no, key, value);
    
    err = kvdb_put(&db, "hw", "nb");
    if(err)
        my_panic("kvdb put failed", err); 


    err = kvdb_put(&db, key, "ying");
    if(err)
        my_panic("kvdb put failed", err); 
    
    value = kvdb_get(&db, key);
    if(!value)
        my_panic("kvdb get failed", 0);
    printf("Thread %d: [%s]: [%s]\n", no, key, value);

    value = kvdb_get(&db, "hw");
    if(!value)
        my_panic("kvdb get failed", 0);
    printf("Thread %d: [hw]: [%s]\n", no, value);
    
    //printf("Thread %d: close\n", no);
    //err = kvdb_close(&db);
    //if(err)
    //    my_panic("kvdb close failed", err); 
    
    
    free(value);
    return NULL;
}

int process_test(int no) {
    int rc, t;
    pthread_t thread[NUM_THREADS];
    for(t = 0; t < NUM_THREADS; t++) {
        printf("Creating thread %d\n", t);
        rc = pthread_create(&thread[t], NULL, thread_test, &t);
        if (rc)
            printf("ERROR; return code is %d\n", rc);
    }
    for(t = 0; t < NUM_THREADS; t++ )
        pthread_join(thread[t], NULL);
    
    int err = kvdb_close(&db);
    if(err)
        my_panic("kvdb close failed", err); 
}

int main() {
    pid_t pid = fork();
    if(pid == 0) {
        process_test(0);
    }
    else {
        pid_t ppid = fork();
        if(ppid == 0)
            process_test(2);
        else 
            process_test(1);
    }
}
