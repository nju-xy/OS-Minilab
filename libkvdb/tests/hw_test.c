#include <kvdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>

#define NUMTHREADS 5
#define TESTNUM 200
#define FILESIZE 1000

void panic(char *msg){
    printf("%s\n", msg);
    assert(0);
}


kvdb_t db;
void test1(int testnum){
    char *key = (char *)malloc(sizeof(char)*FILESIZE);
    char *value;

    /*kvdb_t db;*/
    kvdb_open(&db, "a.db"); // BUG: should check for errors
    /*kvdb_put(&db, key, "three-easy-pieces");*/
    char *buf = (char *)malloc(sizeof(char)*FILESIZE);
    for(int i = 0; i < testnum; i++){
        sprintf(buf, "operating-%d-systems", i);
        sprintf(key, "operating-%d-sys-hello", i);
        kvdb_put(&db, buf, key);
    }

    for(int i = 0; i < testnum; i++){
        sprintf(buf, "operating-%d-systems", i);
        sprintf(key, "operating-%d-sys-hello-world", i);
        if((i  % 2) == 0)
            kvdb_put(&db, buf, key);
        else{
            sprintf(key, "operating-%d-sys", i);
            kvdb_put(&db, buf, key);
        }
    }
    for(int i = 0; i < testnum; i++){
        sprintf(buf, "operating-%d-systems", i);
        value = kvdb_get(&db, buf);
        printf("[%s]: [%s]\n", buf, value);
        free(value);
    }
    free(key);
    free(buf);
    kvdb_close(&db);
}

void* thread_test(void * data){
    int chret;
    chret = kvdb_open(&db, "a.db");
    if(chret < 0){
        panic("open file failed in tests");
    }
    char *key = (char *)malloc(sizeof(char)*FILESIZE);
    char *buf = (char *)malloc(sizeof(char)*FILESIZE);
    char *value;
    uintptr_t no = (uintptr_t)data;
    for(int i = 0; i < TESTNUM; i++){
        sprintf(key, "operating-%d-system", i);
        sprintf(buf, "operating-%d-system", i);
        kvdb_put(&db, key, buf);
    }
    for(int i = 0; i < TESTNUM; i++){
        if((i % 2) == 0){
            sprintf(key, "operating-%d-system", i);
            sprintf(buf, "operating-%d-system-hello", i);
            kvdb_put(&db, key, buf);
        }
    }
    for(int i = 0; i < TESTNUM; i++){
        sprintf(key, "operating-%d-system", i);
        value = kvdb_get(&db, key);
        printf("[%s]: [%s]: [thread-%ld]\n", key, value, (unsigned long)no);
        free(value);
    }
    free(key);
    free(buf);
    /*chret = kvdb_close(&db);*/
    /*if(chret < 0){*/
        /*panic("close file failed in tests");*/
    /*}*/
}

int pthread_test(){
    int rc;
    uintptr_t t;
    pthread_t thread[NUMTHREADS];
    for(t = 0; t < NUMTHREADS; t++){
        printf("Creating Thread %ld\n", (unsigned long)t + 1);
        rc = pthread_create(&thread[t], NULL, thread_test, (void *)(t));
        if(rc){
            printf("ERROR, return code is %d\n", rc);
            panic("ERROR");
        }
    }
    for(t = 0; t < NUMTHREADS; t++){
        pthread_join(thread[t], NULL);
    }
    /*assert(0);*/
    int err = kvdb_close(&db);
    if(err == -1){
        panic("close file failed in tests");
    }
}

int main(){
    /*test1(100);*/

    pid_t pid = fork();
    if(pid == 0){
        pthread_test();
    }else{
        pid_t ppid = fork();
        if(ppid == 0)
            pthread_test();
        else
            pthread_test();
    }

    /*pthread_test(1);*/

    /*
    kvdb_open(&db, "a.db");
    const char *key = "operating-systems";
    kvdb_put(&db, key, "three-easy-pieces");
    kvdb_put(&db, key, "three-easy-pieces-pk");
    kvdb_put(&db, key, "three-easy");
    kvdb_put(&db, "helloworld", "three-easy-pieces");
    char *value = kvdb_get(&db, key);
    printf("[%s]:[%s]\n", key, value);
    free(value);
    kvdb_close(&db);
    */
    return 0;
}
