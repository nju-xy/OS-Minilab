#ifndef __VFS_H__
#define __VFS_H__

#include "nanos.h"

#define RD_BLK_SZ 4096

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// FLAGS:
// lookup flags
#define FLAG_ADD_REF 0x10000000
#define FLAG_DELETE 0x20000000
#define FLAG_CREATE 0x80000000
#define FLAG_DIR 0x40000000

int proc_lookup(const char * path);
int proc_read(const char * path, void * buf, size_t nbytes);

typedef struct file {
    int refcnt; // 引用计数
    off_t inode_off;
    const char *name;//为了procfs偷懒用的
    uint64_t offset;
    device_t *dev;
    struct filesystem *fs;
} file_t;

typedef struct rd_header {
    int occupied;
    off_t free_start;
    off_t next;
    //off_t data;//这些都是块内偏移量
}rd_header_t;

#define FILE 0
#define DIR 1

typedef struct inode {
    int refcnt;
    int linkcnt;
    int unlink;
    int type;
    off_t inode_off;
    off_t size;
    off_t file;       // 对应文件的块的偏移量
    struct filesystem *fs;
    struct inodeops *ops; // 在inode被创建时，由文件系统的实现赋值
                     // inode ops也是文件系统的一部分
} inode_t;

typedef struct inodeops {
    int (*open)(file_t *file, int flags);
    int (*close)(file_t *file);
    ssize_t (*read)(file_t *file, char *buf, size_t size);
    ssize_t (*write)(file_t *file, const char *buf, size_t size);
    off_t (*lseek)(file_t *file, off_t offset, int whence);
    int (*mkdir)(const char *name);
    int (*rmdir)(const char *name);
    int (*link)(const char *name, inode_t *inode);
    int (*unlink)(const char *name);
    // 你可以自己设计readdir的功能
} inodeops_t;

typedef struct directory {
    char name[32];
    off_t son_inode[64];
    char son_name[64][32];
} dir_t;

typedef struct fsops {
    void (*init)(struct filesystem *fs, const char *name, device_t *dev);
    off_t (*lookup)(struct filesystem *fs, const char *path, int flags);
    int (*close)(struct inode *inode);
} fsops_t;

fsops_t fs_ops;
inodeops_t inode_ops;

typedef struct filesystem {
    const char* name;
    fsops_t *ops;
    device_t *dev;
    off_t root_header; // inode的块的起始偏移量
} filesystem_t;

typedef struct {
    void (*init)();
    int (*access)(const char *path, int mode);
    int (*mount)(const char *path, filesystem_t *fs);
    int (*unmount)(const char *path);
    int (*mkdir)(const char *path);
    int (*rmdir)(const char *path);
    int (*link)(const char *oldpath, const char *newpath);
    int (*unlink)(const char *path);
    int (*open)(const char *path, int flags);
    ssize_t (*read)(int fd, void *buf, size_t nbyte);
    ssize_t (*write)(int fd, void *buf, size_t nbyte);
    off_t (*lseek)(int fd, off_t offset, int whence);
    int (*close)(int fd);
} MODULE(vfs);

#endif
