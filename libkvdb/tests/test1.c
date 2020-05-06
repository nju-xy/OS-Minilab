#include "kvdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>


void my_panic(char* str, int err) {
    if(err)
        printf("%s: %d\n", str, err);
    else
        printf("%s\n", str);
    assert(0);
}

kvdb_t db;
int main() {
    // kvdb_t db;
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
    printf("[%s]: [%s]\n", key, value);
    
    err = kvdb_put(&db, "hw", "nb");
    if(err)
        my_panic("kvdb put failed", err); 


    err = kvdb_put(&db, key, "ying");
    if(err)
        my_panic("kvdb put failed", err); 
    
    value = kvdb_get(&db, key);
    if(!value)
        my_panic("kvdb get failed", 0);
    printf("[%s]: [%s]\n", key, value);

    value = kvdb_get(&db, "hw");
    if(!value)
        my_panic("kvdb get failed", 0);
    printf("[hw]: [%s]\n", value);
    
    err = kvdb_close(&db);
    if(err)
        my_panic("kvdb close failed", err); 
    
    
    free(value);
    return 0;
}
