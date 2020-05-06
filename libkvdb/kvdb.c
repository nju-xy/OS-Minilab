#include "kvdb.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

// #define CRASH_TEST

// #define _FILE_OFFSET_BITS 64
#define MAX_KEY_LEN 128
#define MAX_VAL_LEN (16 * (1 << 20))
#define MIN_VAL_SZ 128 
// 如果出现任何错误(文件无权限创建、文件无权限写入、已经关闭的db等)，
// kvdb_open, kvdb_close, kvdb_put均返回非零值，返回0意味着操作成功执行

// 仅使用append only的日志文件作为数据库, 
// 记录所有put, 以一个'\n'为结束符
extern int errno ;

#ifdef CRASH_TEST
void may_crash() {
    int not_crash = (rand() % 5);
    if(!not_crash) {
        printf("BOOM!\n");
        exit(0);
    }
}
#endif

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void crash_check(int fd);
void crash_handle(int fd) {
    printf("crash\n");
    off_t off = lseek(fd, -1, SEEK_END);
    if(off <= 0)
        return; // 数据库为空
    char temp[2] = {};
    int flag = 0;
    while(read(fd, temp, 1)) {
        if(temp[0] == '\3') {
            write(fd, "\0", 1); // 终止符后面写一位'\0'
            off += 2;// 此后截断, 截断地点为终止符后一位
            // printf("ftrucate %x\n", (int)off);
            ftruncate(fd, off);
            flag = 1;
            break;
        }
        off = lseek(fd, -2, SEEK_CUR);
        if(off <= 0)
            break;
    }
    off = lseek(fd, 0, SEEK_CUR);
    if(!flag && off <= 0)
        ftruncate(fd, 0);

    crash_check(fd);
}

void crash_check(int fd) {
    if(fd <= 0)
        return;
    char temp[5] = {};
    // 日志崩溃检测
    if(lseek(fd, -2, SEEK_END) > 0) {
    // 正常状态: 以结束符结尾, 或者为空
        read(fd, temp, 1);
        if(temp[0] != '\3') {
            crash_handle(fd);
        }
    }
}

int kvdb_open(kvdb_t *db, const char *filename) {
    // 打开filename数据库文件(例如filename指向"a.db")
    // 并将信息保存到db中。如果文件不存在，则创建，
    // 如果文件存在，则在已有数据库的基础上进行操作
    
    pthread_mutex_lock(&lock);
   
    if(db->fd > 0) {
        pthread_mutex_unlock(&lock);
        return 0;
    }

    db->fd = open(filename, O_RDWR | O_CREAT, 0666);
    int ret = db->fd;
    
    if(ret < 0)
        return ret;//文件创建失败
    
    // assert(flock(db->fd, LOCK_EX) == 0);
    flock(db->fd, LOCK_EX);

    crash_check(db->fd);

    // assert(flock(db->fd, LOCK_UN) == 0);
    flock(db->fd, LOCK_UN);
    pthread_mutex_unlock(&lock);
    
    return 0;
}

int kvdb_close(kvdb_t *db) {
    // kvdb_close关闭数据库并释放相关资源
    pthread_mutex_lock(&lock);
    int fd = db->fd; 
    if(fd < 0) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    
    printf("close db\n");
    db->fd = -1;

    int ret = close(fd);
    pthread_mutex_unlock(&lock);
    
    return ret;    
}

int write_str(int fd, const char *str) {
    int len = strlen(str);
    int ret = write(fd, str, sizeof(char) * len);
    if(ret <= 0)
        return ret;
    write(fd, "\0", sizeof(char));
    return ret;
}

int write_log(int fd, const char *key, const char *value) {
    int ret0 = lseek(fd, 0, SEEK_END);
    int ret1 = write_str(fd, key);
    int ret2 = write_str(fd, value);
    int len1 = (int)strlen(key);
    int len2 = (int)strlen(value);
    char l1[5] = {};
    char l2[5] = {};
    sprintf(l1, "%4.x", len1);
    sprintf(l2, "%4.x", len2);
#ifdef CRASH_TEST
    may_crash();
#endif
    int ret3 = write_str(fd, l1);
    int ret4 = write_str(fd, l2);

    if(ret0 < 0 || ret1 <= 0 || ret2 <= 0 || ret3 <= 0 || ret4 <= 0) {
        return 0;
    }
#ifdef CRASH_TEST
    may_crash();
#endif
    sync();
    
#ifdef CRASH_TEST
    may_crash();
#endif
    int ret = write_str(fd, "\3");
    sync();
    
    if(ret <= 0) {
        return 0;
    }

    return 1;
}

char *lockless_get(kvdb_t *db, const char *key);

int lockless_put(kvdb_t *db, const char *key, const char *value) {
    // 崩溃检测
    // crash_check(db->fd);

    // 如果原来key就对应value, 就不用写了, 
    // 而且get中带有一次日志崩溃检测
    char *oldv = lockless_get(db, key);
    if(oldv && !strcmp(oldv, value)) {
        free(oldv);
        return 0;
    }
    free(oldv);


    // 写日志
    int wr = write_log(db->fd, key, value);
    if(wr <= 0) {
        printf("log failed\n");
        return -1;
    }
    
    // 崩溃检测
    crash_check(db->fd);
    
    return 0;
}

int kvdb_put(kvdb_t *db, const char *key, const char *value) {
    // 建立key到value的映射，
    // 如果把db看成是一个std::map<std::string,std::string>，
    // 则相当于执行db[key] = value;
    // 因此如果在kvdb_put执行之前db[key]已经有一个对应的字符串，
    // 它将被value覆盖
    
    pthread_mutex_lock(&lock);
    
    // 数据库已经关闭
    if(db->fd < 0) {
        pthread_mutex_unlock(&lock);
        return -1;
    }
    
    // assert(flock(db->fd, LOCK_EX) == 0);
    flock(db->fd, LOCK_EX);
    
    int ret = lockless_put(db, key, value);

    // assert(flock(db->fd, LOCK_UN) == 0);
    flock(db->fd, LOCK_UN);

    pthread_mutex_unlock(&lock);
    return ret;
}

char *lockless_get(kvdb_t *db, const char *key) {
    //printf("get start\n");
    int fd = db->fd;
    
    if(lseek(fd, -2, SEEK_END) <= 0) {
        // 日志为空
        return NULL;
    }

    char *k = (char*)malloc(MAX_KEY_LEN * sizeof(char));
    char *v = (char*)malloc(MAX_VAL_LEN * sizeof(char));
    if((uintptr_t)k == 0) {
        printf("malloc failed\n");
        return NULL;
    }
    if((uintptr_t)v == 0) {
        free(k);
        printf("malloc failed\n");
        return NULL;
    }
    
    char l1[5] = {}; 
    char l2[5] = {}; 

    // 崩溃检测
    crash_check(db->fd);
    
    int err = 0;
    while(1) {
        // 从一个终止符后到另一个终止符后
        int ret;
        ret = lseek(fd, -6, SEEK_CUR);
        err += (ret <= 0);
        // assert(!err);
        ret = read(fd, l2, 4);
        err += (ret != 4);
        // assert(!err);
        ret = lseek(fd, -9, SEEK_CUR);
        err += (ret <= 0);
        // assert(!err);
        ret = read(fd, l1, 4);
        err += (ret != 4);
        // assert(!err);
        
        int len1, len2;
        sscanf(l1, "%x", &len1);
        sscanf(l2, "%x", &len2);
        l1[4] = '\0';
        l2[4] = '\0';

        ret = lseek(fd, 0, SEEK_CUR);
        //printf("lseek return %x\n", ret);
        ret = lseek(fd, - 5 - len2, SEEK_CUR);
        err += (ret <= 0);
        // assert(!err);
        ret = read(fd, v, len2 + 1);
        err += (ret != len2 + 1);
        /*if(err) {
            int a = (int)lseek(fd, 0, SEEK_CUR);
            int b = (int)lseek(fd, 0, SEEK_END);
            lseek(fd, a, SEEK_SET);
            printf("l: %x, off: %x, filesize: %x, return %d\n", len2 + 1, a, b, ret);
            printf("v addr: %x\n", (int)(uintptr_t)&v[0]);
            fprintf(stderr, "Error: %s\n", strerror(errno));
        }*/
        // assert(!err);
        ret = lseek(fd, - len2 - 2 - len1, SEEK_CUR);
        err += (ret < 0);
        // assert(!err);
        ret = read(fd, k, len1 + 1);
        err += (ret != len1 + 1);
        // assert(!err);
        if(err) break;
        off_t off = lseek(fd, - len1 - 3, SEEK_CUR);
        //printf("get:[%x]: %s; [%x]: %s\n", len1, k, len2, v);
        if(off <= 0)
            break;
        if(!strcmp(k, key)) {
            break;
        }

        char temp[5] = {};
        ret = read(fd, temp, 1);
        err += (ret != 1);
        // assert(!err);
        if(err) break;
        err = (temp[0] != '\3');
        // assert(!err);
        if(err) break;
    }
    free((void*)k);
    if(err)
        return NULL;//lockless_get(db, key);
    //printf("get ended\n");
    return v;
}

char *kvdb_get(kvdb_t *db, const char *key) {
    // 获取key对应的value，相当于返回db[key]。
    // 返回的value是通过动态内存分配实现的
    // (例如malloc或strdup分配的空间)，
    // 因此在使用完毕后需要调用free释放。
    // 如果db不合法、内存分配失败或key不存在，则kvdb_get返回空指针。
    
    pthread_mutex_lock(&lock);
    
    // 数据库已经关闭
    if(db->fd < 0) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    
    flock(db->fd, LOCK_EX);
    char *ret = lockless_get(db, key);
    flock(db->fd, LOCK_UN);
    pthread_mutex_unlock(&lock);
 
    return ret;
}
