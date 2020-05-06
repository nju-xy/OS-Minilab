#include "common.h"
// supported commands:
//   ls
//   cd /proc
//   cat filename
//   mkdir /bin
//   rm /bin/abc
//   ...

extern spinlock_t print_lk;
#define NR_MOUNT_POINTS 20
extern struct Mount_point{
    char name[64];
    filesystem_t *fs;
} mount_point[NR_MOUNT_POINTS];
extern filesystem_t * blkfs;

int str_to_num(char *str) {
    int len = strlen(str);
    int ret = 0;
    if(str[0] == '-')
        return -str_to_num(str + 1);
    for (int i = 0; i < len; i++) {
        ret = ret * 10 + str[i] - '0';
    }
    return ret;
}

int readbuf(char ** argv, char * buf) {
    int len = strlen(buf);
    int argc = 0;
    int idx = 0;
    for(int i = 0; i < len; ++i) {
        if(buf[i] == ' ') {
            argc++;
            idx = 0;
        }
        else {
            argv[argc][idx] = buf[i];
            idx++;
        }
    }
    /*for (int i = 0; i < argc + 1; i++) {
        Log("%s", argv[i]);
    }*/
    argc++;
    return argc;
}

extern task_t *cur_task[4];

void path_preprocessing(char *path, char *new_path) {
    if(strlen(path) == 0)
	path[0] = '.';
    if(strlen(cur_task[_cpu()]->pwd) == 0)
        cur_task[_cpu()]->pwd[0] = '/';
    if(path[0] == '.' && path[1] != '.') {
        strncpy(new_path, cur_task[_cpu()]->pwd, 64);
        if(strlen(path) == 1 || strlen(path) == 2)
            return;
        else if(strlen(new_path) != 1)
            strcat(new_path, path + 1);
	    else
	        strcat(new_path, path + 2);
    }   
    else if(path[0] == '.' && path[1] == '.') {
        strncpy(new_path, cur_task[_cpu()]->pwd, 64);
        int len = strlen(new_path);
        for (int i = len - 1; i >= 0; i--) {
            if(new_path[i] == '/') {
                new_path[i] = '\0';
                break;
            }   
            new_path[i] = '\0';
        }
	if(strlen(path) == 2)
	    new_path[0] = '/';
        strcat(new_path, path + 2);
        Log("newpath: %s", new_path);
    }
    else {
        strncpy(new_path, path, 64);
    }
}

void cd(char *path, char *buf, int *buf_off) {
    char new_path[64] = {};
    path_preprocessing(path, new_path);
    off_t inode_off = blkfs->ops->lookup(blkfs, new_path, FLAG_DIR);
    if(inode_off <= 0) {
        sprintf(buf + * buf_off, "Invalid path");
        *buf_off = strlen(buf);
        return;
    }
    strncpy(cur_task[_cpu()]->pwd, new_path, 64);
    sprintf(buf + *buf_off, "PWD: %s", new_path);
    *buf_off = strlen(buf);
}

void ls(char ** argv, char * buf, int * buf_off) {
    char path[64] = {};
    path_preprocessing(argv[1], path);
    
    Log("path: %s", path);
    filesystem_t *fs = blkfs;
    int no = find_mount_point(fs, path);
    if(no == -2) {
        //sprintf(buf + * buf_off, "Invalid path");
        //*buf_off = strlen(buf);
        return;
    }
    off_t inode_off = fs->ops->lookup(fs, path, FLAG_DIR);
    if(inode_off <= 0) {
        sprintf(buf + * buf_off, "Invalid path");
        *buf_off = strlen(buf);
        return;
    }
    inode_t * dir_inode = pmm->alloc(sizeof(inode_t));
    device_t *dev = fs->dev;
    dev->ops->read(dev, inode_off, dir_inode, sizeof(inode_t));
    if(dir_inode->file <= 0) {
        sprintf(buf + *buf_off, "Invalid path");
        *buf_off = strlen(buf);
        return;
    }
    dir_t *dir = pmm->alloc(sizeof(dir_t));
    dev->ops->read(dev, dir_inode->file + sizeof(rd_header_t), dir, sizeof(dir_t));
    Log("in dir %s:", dir->name);
    for (int i = 0; i < 64; i++) {
        if(dir->son_inode[i] > 0) {
            sprintf(buf + *buf_off, "%s ", dir->son_name[i]);
            *buf_off = strlen(buf);
        }                    
    }
    pmm->free(dir_inode);
    pmm->free(dir);
}

void shell_thread(void *tty_id) {
    char buf[128] = {};
    int buf_off = 0;
    int ttyid = (int)tty_id;
    char ttyno[12] = {};
    int got_a_line = 0; 
    sprintf(buf, "/dev/tty%d", ttyid);
    sprintf(ttyno, "(tty%d) $ ", ttyid);
    int stdin = vfs->open(buf, 0);
    int stdout = vfs->open(buf, 0);
    ssize_t nread = 0;
    char *argv[5];
    for (int i = 0; i < 5; i++) {
        argv[i] = pmm->alloc(64);
    }   
    while (1) {
        if (got_a_line) {
            got_a_line = 0;
            int argc = readbuf(argv, buf);
            if(argc == 0)
                continue;
            buf_off = 0;
            if((strncmp(argv[0], "cd", 64) == 0) && argc == 2) {
                cd(argv[1], buf, &buf_off);
            }
            else if((strncmp(argv[0], "ls", 64) == 0) && argc <= 2) {
                ls(argv, buf, &buf_off);
            }
            else if((strncmp(argv[0], "mkdir", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
		        int ret = vfs->mkdir(path);
                if(ret)
                    sprintf(buf + buf_off, "mkdir %s succeeded", path);
                else
                    sprintf(buf + buf_off, "mkdir %s failed", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "rmdir", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int ret = vfs->rmdir(path);
                if(ret)
                    sprintf(buf + buf_off, "rmdir %s succeeded", path);
                else
                    sprintf(buf + buf_off, "rmdir %s failed", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "open", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int ret = vfs->open(path, 0);
                if(ret >= 0) {
                    sprintf(buf + buf_off, "open %s succeeded, fd: %d", path, ret);
                }
                else
                    sprintf(buf + buf_off, "open %s failed", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "close", 64) == 0) && argc == 2){
                int fd = str_to_num(argv[1]);
                int ret = vfs->close(fd);
                if(ret) {
                    sprintf(buf + buf_off, "close %d succeeded", fd);
                }
                else
                    sprintf(buf + buf_off, "close %d failed", fd);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "read", 64) == 0) && argc == 3){
                int fd = str_to_num(argv[1]);
                int nbyte = str_to_num(argv[2]);
                if(nbyte > 100)
                    nbyte = 100;
                char * temp = pmm->alloc(128);
                int ret = vfs->read(fd, temp, nbyte);
                if(ret > 0) {
                    sprintf(buf + buf_off, "get:", fd);
                    for (int i = 0; i < nbyte; i++) {
                        sprintf(buf + buf_off, "%c", temp[i]);
                        buf_off++;
                    }
                }
                else
                    sprintf(buf + buf_off, "read %d failed", fd);
                pmm->free(temp);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "cat", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int fd = vfs->open(path, 0);
                Log("fd: %d", fd);
                if(fd >= 0) {
                    int nbyte = 100;
                    char * temp = pmm->alloc(128);
                    int ret = vfs->read(fd, temp, nbyte);
                    if(ret > 0) {
                        for (int i = 0; i < nbyte; i++) {
                            sprintf(buf + buf_off, "%c", temp[i]);
                            buf_off++;
                        }
                    }
                    else
                        sprintf(buf + buf_off, "cat %s failed", path);
                    pmm->free(temp);
                    buf_off = strlen(buf);
                    vfs->close(fd);
                }
                else {
                    sprintf(buf + buf_off, "cat %s failed", path);
                    buf_off = strlen(buf);
                }
            }
            else if((strncmp(argv[0], "write", 64) == 0) && argc == 4){
                int fd = str_to_num(argv[1]);
                int nbyte = str_to_num(argv[3]);
                int ret = vfs->write(fd, argv[2], nbyte);
                if(ret > 0) {
                    sprintf(buf + buf_off, "write %d %d bytes", fd, ret);
                }
                else
                    sprintf(buf + buf_off, "write %d failed", fd);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "lseek", 64) == 0) && argc == 4){
                int flag = 0;
                int fd = str_to_num(argv[1]);
                off_t off = str_to_num(argv[2]);
                int whence = 0;
                if(strncmp(argv[3], "SEEK_SET", 64) == 0)
                    whence = 0;
                else if(strncmp(argv[3], "SEEK_CUR", 64) == 0)
                    whence = 1;
                else if(strncmp(argv[3], "SEEK_END", 64) == 0)
                    whence = 2;
                else {
                    flag = 1;
                } 
                if(!flag) {
                    int ret = vfs->lseek(fd, off, whence);
                    if(ret >= 0)
                        sprintf(buf + buf_off, "lseek %d, now offset is %d", fd, ret);
                    else
                        sprintf(buf + buf_off, "lseek %d failed", fd);
                }
                else {
                    sprintf(buf + buf_off, "lseek %d failed", fd);
                } 
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "touch", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int ret = vfs->open(path, FLAG_CREATE);
                if(ret >= 0) {
                    vfs->close(ret);
                    sprintf(buf + buf_off, "touch %s succeeded", path);
                }
                else
                    sprintf(buf + buf_off, "touch %s failed", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "access", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int ret = vfs->access(path, 0);
                if(ret == 1) {
                    sprintf(buf + buf_off, "%s is accessible", path);
                }
                else
                    sprintf(buf + buf_off, "%s is not accessible", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "unmount", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int ret = vfs->unmount(path);
                if(ret == 1)
                    sprintf(buf + buf_off, "unmount %s succeeded", path);
                else
                    sprintf(buf + buf_off, "unmount %s failed", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "unlink", 64) == 0) && argc == 2){
		        char path[64] = {};
    		    path_preprocessing(argv[1], path);                
                int ret = vfs->unlink(path);
                if(ret == 0)
                    sprintf(buf + buf_off, "unlink %s succeeded", path);
                else
                    sprintf(buf + buf_off, "unlink %s failed", path);
                buf_off = strlen(buf);
            }
            else if((strncmp(argv[0], "link", 64) == 0) && argc == 3){
		        char path1[64] = {};
    		    path_preprocessing(argv[1], path1);                
		        char path2[64] = {};
    		    path_preprocessing(argv[2], path2);                
                int ret = vfs->link(path1, path2);
                if(ret == 0)
                    sprintf(buf + buf_off, "link %s %s succeeded", path1, path2);
                else
                    sprintf(buf + buf_off, "link %s %s failed", path1, path2);
                buf_off = strlen(buf);
            }
            else {
                sprintf(buf + buf_off, "Invalid command");
                buf_off = strlen(buf);
            }
            sprintf(buf + buf_off, "\n");
            vfs->write(stdout, buf, strlen(buf));
            memset(buf, 0, sizeof(buf));
            for (int i = 0; i < 5; i++) {
                memset(argv[i], 0, 64);
            }
        } else {
            vfs->write(stdout, ttyno, strlen(ttyno));
            nread = vfs->read(stdin, buf, sizeof(buf));
            if(nread > 0) {
                buf[nread - 1] = '\0';
                got_a_line = 1;
            }

        }
    } 
    for (int i = 0; i < 5; i++) {
        pmm->free(argv[i]);
    }
}
