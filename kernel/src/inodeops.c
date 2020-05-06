//感觉很不好用，因此这部分都没写，直接在vfs.c中实现了这些功能
#include "common.h"
int inode_open(file_t *file, int flags) {
    TODO();
    return 0;
}

int inode_close(file_t *file) {
    TODO();
    return 0;
}

ssize_t inode_read(file_t *file, char *buf, size_t size) {
    TODO();
    return 0;
}

ssize_t inode_write(file_t *file, const char *buf, size_t size) {
    TODO();
    return 0;
}

off_t inode_lseek(file_t *file, off_t offset, int whence) {
    TODO();
    return 0;
}

int inode_mkdir(const char *name) {
    TODO();
    return 0;
}

int inode_rmdir(const char *name) {
    return 0;
}

int inode_link(const char *name, inode_t *inode) {
    TODO();
    return 0;
}

int inode_unlink(const char *name) {
    TODO();
    return 0;
}

inodeops_t inode_ops = {
    .open = inode_open,
    .close = inode_close,
    .read = inode_read,
    .write = inode_write,
    .lseek = inode_lseek,
    .mkdir = inode_mkdir,
    .rmdir = inode_rmdir,
    .link = inode_link,
    .unlink = inode_unlink,
};
