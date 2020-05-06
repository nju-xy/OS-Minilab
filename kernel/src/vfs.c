#include "common.h"

spinlock_t vfs_lk;
extern spinlock_t print_lk;
//extern task_t *Task[MAX_NR_TASKS];
extern task_t *cur_task[4];
#define NR_MOUNT_POINTS 20

struct Mount_point{
    char name[64];
    filesystem_t *fs;
    int unmount;
} mount_point[NR_MOUNT_POINTS] = {};

filesystem_t *blkfs = NULL;
filesystem_t *devfs = NULL;
filesystem_t *procfs = NULL;

extern fsops_t blk_fs_ops;
extern fsops_t dev_fs_ops;
void vfs_init() {
    kmt->spin_init(&vfs_lk, "vfs_lock");
    
    device_t *rd1 = dev_lookup("ramdisk1");
    blkfs = (filesystem_t *)(pmm->alloc(sizeof(filesystem_t)));
    blkfs->ops = &blk_fs_ops;
    blkfs->ops->init(blkfs, "blkfs", rd1);

    // /dev/tty1-4, /dev/ramdisk0, /dev/ramdisk1
    devfs = (filesystem_t *)(pmm->alloc(sizeof(filesystem_t)));
    devfs->ops = &dev_fs_ops;
    devfs->ops->init(devfs, "devfs", NULL);
    vfs->mkdir("/dev");
    int fd = vfs->open("/dev/tty1", FLAG_CREATE);
    vfs->close(fd);
    fd = vfs->open("/dev/tty2", FLAG_CREATE);
    vfs->close(fd);
    fd = vfs->open("/dev/tty3", FLAG_CREATE);
    vfs->close(fd);
    fd = vfs->open("/dev/tty4", FLAG_CREATE);
    vfs->close(fd);
    fd = vfs->open("/dev/ramdisk0", FLAG_CREATE);
    vfs->close(fd);
    fd = vfs->open("/dev/ramdisk1", FLAG_CREATE);
    vfs->close(fd);
    vfs->mount("/dev", devfs);
    
    vfs->mkdir("/proc");
    char temp[20] = {};
    for (int i = 0; i < MAX_NR_TASKS; i++) {
        sprintf(temp, "/proc/%d", i);
        fd = vfs->open(temp, FLAG_CREATE);
        vfs->close(fd);
    }
    fd = vfs->open("/proc/meminfo", FLAG_CREATE);
    vfs->close(fd);
    fd = vfs->open("/proc/cpuinfo", FLAG_CREATE);
    vfs->close(fd);
    
    procfs = (filesystem_t *)(pmm->alloc(sizeof(filesystem_t)));
    procfs->ops = &dev_fs_ops;
    procfs->ops->init(procfs, "procfs", NULL);
    vfs->mount("/proc", procfs);
    vfs->mkdir("/Documents");
    fd = vfs->open("/Documents/README", FLAG_CREATE);
    vfs->write(fd, "This is my operating system.", 28);
    vfs->close(fd);
}


int find_mount_point(filesystem_t * fs, const char * path) {
    for (int i = 0; i < NR_MOUNT_POINTS; i++) {
        if(mount_point[i].fs == NULL)
            continue;
        char * name = mount_point[i].name;
        int len = strlen(name);
        if(strncmp(path, name, len) == 0) {
            if(mount_point[i].unmount == 1)
                return -2;
            fs = mount_point[i].fs;
            //path = path + strlen(name);
            Log("find %s, fs: %s, path %s", name, mount_point[i].fs->name, path);
            return i;
        }   
    }
    return -1;
}


int vfs_access(const char *path, int mode) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("access %s", path);
    kmt->spin_unlock(&print_lk);
    
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    // 找到它所在的文件系统fs
    filesystem_t *fs = blkfs;
    int no = find_mount_point(fs, path);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(no >= 0) {
        fs = mount_point[no].fs;
        path = path + strlen(mount_point[no].name) + 1;
    }
    if(strncmp(fs->name, "devfs", 32) == 0) {
        Log("dev %s", path);
        device_t *dev = dev_lookup(path);
        if(!dev) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        kmt->spin_unlock(&vfs_lk);
        return 1;
    }
    if(strncmp(fs->name, "procvfs", 32) == 0) {
        Log("dev %s", path);
        int proc_ret = proc_lookup(path);
        kmt->spin_unlock(&vfs_lk);
        return proc_ret;
    }
    //lookup
    off_t off = fs->ops->lookup(fs, path, 0);
    if(!off) {
        kmt->spin_lock(&print_lk);
        Log("cannot access");
        kmt->spin_unlock(&print_lk);
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    kmt->spin_unlock(&vfs_lk);
    return 0;
}

int vfs_mount(const char *path, filesystem_t *fs) {
    kmt->spin_lock(&vfs_lk);
     
    kmt->spin_lock(&print_lk);
    Log("mount %s", path);
    kmt->spin_unlock(&print_lk);

    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    blkfs->ops->lookup(blkfs, path, FLAG_DIR | FLAG_CREATE);
    int no = -1;
    for (int i = 0; i < NR_MOUNT_POINTS; i++) {
        if(mount_point[i].fs == NULL) {
            no = i;
            break;
        }
    }
    if(no < 0) {
        kmt->spin_lock(&print_lk);
        Log("No free mount points.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    mount_point[no].fs = fs;
    mount_point[no].unmount = 0;
    strncpy(mount_point[no].name, path, 64);

    kmt->spin_unlock(&vfs_lk);
    return 1;
}

int vfs_unmount(const char *path){
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("unmount %s", path);
    kmt->spin_unlock(&print_lk);
    
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    int no = -1;
    for (int i = 0; i < NR_MOUNT_POINTS; i++) {
        if(mount_point[i].fs)
            Log("%s", mount_point[i].name);
        if(mount_point[i].fs != NULL && strncmp(mount_point[i].name, path, 64) == 0) {
            no = i;
            break;
        }
    }
    if(no < 0) {
        kmt->spin_lock(&print_lk);
        Log("No such mount points.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    /*
    if(strncmp(path, "/dev", 64) == 0) {
        vfs->unlink("/dev/tty1");
        vfs->unlink("/dev/tty2");
        vfs->unlink("/dev/tty3");
        vfs->unlink("/dev/tty4");
        vfs->unlink("/dev/ramdisk0");
        vfs->unlink("/dev/ramdisk1");
        vfs->rmdir("/dev");
    }
    if(strncmp(path, "/dev", 64) == 0) {
        vfs->unlink("/proc/cpuinfo");
        vfs->unlink("/proc/meminfo");
        for (int i = 0; i < MAX_NR_TASKS; i++) {
            char temp[10] = {};
            sprintf(temp, "/proc/%d", i);
            vfs->unlink(temp);
        }
        vfs->rmdir("/proc");
    }*/
    //mount_point[no].fs = NULL;
    //mount_point[no].name[0] = '\0';
    mount_point[no].unmount = 1;

    kmt->spin_unlock(&vfs_lk);
    return 1;
}

int vfs_mkdir(const char *path) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("mkdir %s", path);
    kmt->spin_unlock(&print_lk);
    
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    // 找到它所在的文件系统fs
    filesystem_t *fs = blkfs;
    int no = find_mount_point(fs, path);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    if(no >= 0) {
        fs = mount_point[no].fs;
        path = path + strlen(mount_point[no].name) + 1;
    }
    if(strncmp(fs->name, "procfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    if(strncmp(fs->name, "devfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    //lookup
    off_t off = fs->ops->lookup(fs, path, FLAG_DIR | FLAG_CREATE);
    if(!off) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    kmt->spin_unlock(&vfs_lk);
    return 1;
}

int vfs_rmdir(const char *path) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("rmdir %s", path);
    kmt->spin_unlock(&print_lk);
    
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    // 找到它所在的文件系统fs
    filesystem_t *fs = blkfs;
    int no = find_mount_point(fs, path);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    if(no >= 0) {
        fs = mount_point[no].fs;
        path = path + strlen(mount_point[no].name) + 1;
    }
    if(strncmp(fs->name, "procfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    if(strncmp(fs->name, "devfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    //lookup
    off_t off = fs->ops->lookup(fs, path, FLAG_DIR | FLAG_DELETE);
    kmt->spin_unlock(&vfs_lk);
    return off;
}

int vfs_link(const char *oldpath, const char *newpath) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("link %s to %s", newpath, oldpath);    
    kmt->spin_unlock(&print_lk);
    
    filesystem_t *fs_new = blkfs;
    int no = find_mount_point(fs_new, newpath);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(no >= 0) {
        fs_new = mount_point[no].fs;
        newpath = newpath + strlen(mount_point[no].name) + 1;
    }
    if(strncmp(fs_new->name, "procfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(strncmp(fs_new->name, "devfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    off_t off_new = fs_new->ops->lookup(fs_new, newpath, 0);
    if(off_new > 0) {
        //需要新路径不存在
        Log("new path %s already exist, inode off: %x, link failed", newpath, off_new);
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    filesystem_t *fs_old = blkfs;
    no = find_mount_point(fs_old, oldpath);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(no >= 0) {
        fs_old = mount_point[no].fs;
        oldpath = oldpath + strlen(mount_point[no].name) + 1;
    }
    if(strncmp(fs_old->name, "procfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(strncmp(fs_old->name, "devfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    off_t off_old = fs_old->ops->lookup(fs_old, oldpath, 0);
    if(off_old <= 0) {
        //需要新路径不存在
        Log("old path doesn't exist, link failed");
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    device_t *dev_old = fs_old->dev;
    inode_t *inode_old = pmm->alloc(sizeof(inode_t));
    dev_old->ops->read(dev_old, off_old, inode_old, sizeof(inode_t));

    inode_old->linkcnt ++;
    dev_old->ops->write(dev_old, off_old, inode_old, sizeof(inode_t));
    
    char dirpath[64];
    const char *filename;
    strncpy(dirpath, newpath, 64);
    int len = strlen(dirpath);
    int j;
    for (j = len - 1; j > 0; j--) {
        if(newpath[j] == '/') {
            dirpath[j] = '\0';
            break;
        }
        else {
            dirpath[j] = '\0';
        }
    }
    filename = newpath + j + 1;
    Log("dirpath: %s, filename: %s", dirpath, filename);
    off_t dir_inode_off = fs_new->ops->lookup(fs_new, dirpath, FLAG_DIR);
    if(dir_inode_off <= 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    device_t *dev_new = fs_new->dev;
    inode_t * dir_inode = pmm->alloc(sizeof(inode_t));
    dev_new->ops->read(dev_new, dir_inode_off, dir_inode, sizeof(inode_t));
    if(dir_inode->file <= 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    dir_t *dir = pmm->alloc(sizeof(dir_t));
    dev_new->ops->read(dev_new, dir_inode->file + sizeof(rd_header_t), dir, sizeof(dir_t));
    for (int i = 0; i < 64; i++) {
        if(dir->son_inode[i] == 0) {
	        dir->son_inode[i] = off_old;
	        strncpy(dir->son_name[i], filename, 32);
            Log("%s is son %d of %s", filename, i, dir->name);
	        break;
	    }
    }
    dev_new->ops->write(dev_new, dir_inode->file + sizeof(rd_header_t), dir, sizeof(dir_t));
    pmm->free(dir_inode);
    pmm->free(dir);
    //off_new = fs_old->ops->lookup(fs_new, newpath, FLAG_CREATE);
    //device_t *dev_new = fs_new->dev;
    //inode_t *inode_new = pmm->alloc(sizeof(inode_t));
    //dev_new->ops->read(dev_new, off_new, inode_new, sizeof(inode_t));
    
    //char * rm_buf = pmm->alloc(RD_BLK_SZ);
    //dev_new->ops->write(dev_new, inode_new->file, rm_buf, RD_BLK_SZ);
    //pmm->free(rm_buf);

    //dev_new->ops->write(dev_new, off_new, inode_old, sizeof(inode_t));
    kmt->spin_unlock(&vfs_lk);
    return 0;
}

int vfs_unlink(const char *path) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("unlink %s", path);
    kmt->spin_unlock(&print_lk);
    
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    // 找到它所在的文件系统fs
    filesystem_t *fs = blkfs;
    int no = find_mount_point(fs, path);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(no >= 0) {
        fs = mount_point[no].fs;
        path = path + strlen(mount_point[no].name) + 1;
    }
    if(strncmp(fs->name, "procfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(strncmp(fs->name, "devfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    //lookup
    off_t inode_off = fs->ops->lookup(fs, path, 0);
    if(!inode_off) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }

    device_t *dev = fs->dev;
    inode_t *inode = pmm->alloc(sizeof(inode_t));
    dev->ops->read(dev, inode_off, inode, sizeof(inode_t));
    //Log("refcnt: %d", inode->refcnt);
    if(inode->refcnt == 0 && inode->linkcnt == 1) {// 没人引用，直接删除
        fs->ops->lookup(fs, path, FLAG_DELETE);
    }
    else {//在上级目录文件中去掉这个文件
	if(inode->linkcnt == 1){//只有一个link，标记一下，全close后删除
            inode->unlink = 1;
            //dev->ops->write(dev, inode_off, inode, sizeof(inode_t));
    	}
        inode->linkcnt --;
        dev->ops->write(dev, inode_off, inode, sizeof(inode_t));
        
        char dirpath[64];
        const char *filename;
        strncpy(dirpath, path, 64);
        int len = strlen(path);
        int j;
        for (j = len - 1; j > 0; j--) {
            dirpath[j] = '\0';
            if(path[j] == '/') {
                break;
            }
        }
        filename = path + j + 1;
        Log("dirpath: %s, filename: %s", dirpath, filename);
        off_t dir_inode_off = fs->ops->lookup(fs, dirpath, FLAG_DIR);
        if(dir_inode_off <= 0) {
            kmt->spin_unlock(&vfs_lk);
            return 0;
        }
        
        inode_t * dir_inode = pmm->alloc(sizeof(inode_t));
        dev->ops->read(dev, dir_inode_off, dir_inode, sizeof(inode_t));
        if(dir_inode->file <= 0) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        
        dir_t *dir = pmm->alloc(sizeof(dir_t));
        dev->ops->read(dev, dir_inode->file + sizeof(rd_header_t), dir, sizeof(dir_t));
        for (int i = 0; i < 64 && dir->son_inode[i]; i++) {
            if(dir->son_inode[i] == -1)
                continue;
            if(strncmp(filename, dir->son_name[i], 32) == 0) {
                dir->son_inode[i] = -1;
                Log("%s is son %d of %s", filename, i, dir->name);
                break;
            }
        }
        dev->ops->write(dev, dir_inode->file + sizeof(rd_header_t), dir, sizeof(dir_t));
    }

    kmt->spin_unlock(&vfs_lk);
    return 0;
}

int vfs_open(const char *path, int flags) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("open %s", path);
    kmt->spin_unlock(&print_lk);
    
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("Path not start with /.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    // 找到它所在的文件系统fs
    filesystem_t *fs = blkfs;
    int no = find_mount_point(fs, path);
    if(no == -2) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    if(no >= 0) {
        fs = mount_point[no].fs;
        path = path + strlen(mount_point[no].name) + 1;
    }
    device_t *dev = fs->dev;
    int is_dev = 0;
    int is_proc = 0;
    if(strncmp(fs->name, "devfs", 32) == 0) {
        dev = dev_lookup(path);
        if(!dev) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        is_dev = 1;
    }
    
    if(strncmp(fs->name, "procfs", 32) == 0) {
        if(proc_lookup(path) == 0) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        is_proc = 1;
    }
    
    //lookup
    off_t off = 0;
    if(!is_dev && !is_proc) {
        off = fs->ops->lookup(fs, path, (FLAG_CREATE & flags) | FLAG_ADD_REF);
        if(!off) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
    }

    // 在线程cur_task[_cpu()]的fildes中找到第一个为NULL的位置
    // 使用file = pmm->alloc()分配一个新的file_t；
    int fd = -1;
    for (int i = 0; i < NR_FILE; i++) {
        if(cur_task[_cpu()]->fildes[i] == NULL) {
            cur_task[_cpu()]->fildes[i] = pmm->alloc(sizeof(file_t));
            cur_task[_cpu()]->fildes[i]->refcnt = 0;
            cur_task[_cpu()]->fildes[i]->inode_off = off;
            cur_task[_cpu()]->fildes[i]->offset = 0;
            cur_task[_cpu()]->fildes[i]->fs = fs;
            if(is_dev)
                cur_task[_cpu()]->fildes[i]->dev = dev;
            if(is_proc)
                cur_task[_cpu()]->fildes[i]->name = path;
            fd = i;
            break;
        }
    }
    if(fd == -1) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    kmt->spin_unlock(&vfs_lk);
    return fd;
}

ssize_t vfs_read(int fd, void *buf, size_t nbyte) {
    //TODO();
    kmt->spin_lock(&vfs_lk);
    if(fd < 0 || cur_task[_cpu()]->fildes[fd] == NULL) {
        kmt->spin_lock(&print_lk);
        Log("illegal fd: %d, read failed", fd);
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    kmt->spin_lock(&print_lk);
    Log("read fd[%d] %x bytes from %x", fd, nbyte, cur_task[_cpu()]->fildes[fd]->offset);
    kmt->spin_unlock(&print_lk);
    
    device_t *dev;
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "devfs", 32) == 0) {
        dev = cur_task[_cpu()]->fildes[fd]->dev;
        if(!dev) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        kmt->spin_unlock(&vfs_lk);
        int dev_ret = dev->ops->read(dev, cur_task[_cpu()]->fildes[fd]->offset, buf, nbyte);
        kmt->spin_lock(&vfs_lk);
        if(dev_ret > 0) 
            cur_task[_cpu()]->fildes[fd]->offset += dev_ret;
        kmt->spin_unlock(&vfs_lk);
        return dev_ret;
    }
    
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "procfs", 32) == 0) {
        int proc_ret = proc_read(cur_task[_cpu()]->fildes[fd]->name, buf, nbyte); 
        kmt->spin_unlock(&vfs_lk);
        return proc_ret;
    }
    off_t inode_off = cur_task[_cpu()]->fildes[fd]->inode_off;
    dev = cur_task[_cpu()]->fildes[fd]->fs->dev;
    inode_t *inode = pmm->alloc(sizeof(inode_t));
    dev->ops->read(dev, inode_off, inode, sizeof(inode_t));
    
    if(cur_task[_cpu()]->fildes[fd]->offset + nbyte > inode->size) {
        kmt->spin_lock(&print_lk);
        Log("read too much, exceed file end");
        kmt->spin_unlock(&print_lk);
        nbyte = inode->size - cur_task[_cpu()]->fildes[fd]->offset;
    }
    
    off_t read_off = inode->file + sizeof(rd_header_t) + cur_task[_cpu()]->fildes[fd]->offset;
    ssize_t ret = dev->ops->read(dev, read_off, buf, nbyte);
    if(ret > 0) 
        cur_task[_cpu()]->fildes[fd]->offset += ret;
    pmm->free(inode);
    kmt->spin_unlock(&vfs_lk);
    return ret;
}

ssize_t vfs_write(int fd, void *buf, size_t nbyte) {
    //TODO();
    kmt->spin_lock(&vfs_lk);
    if(fd < 0 || cur_task[_cpu()]->fildes[fd] == NULL) {
        kmt->spin_lock(&print_lk);
        Log("illegal fd: %d, write failed", fd);
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    kmt->spin_lock(&print_lk);
    Log("CPU[%d] write fd[%d] %d bytes from %d", _cpu(), fd, nbyte, cur_task[_cpu()]->fildes[fd]->offset);
    kmt->spin_unlock(&print_lk);
    
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "procfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
   
    device_t *dev;
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "devfs", 32) == 0) {
        dev = cur_task[_cpu()]->fildes[fd]->dev;
        if(!dev) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        kmt->spin_unlock(&vfs_lk);
        int dev_ret = dev->ops->write(dev, cur_task[_cpu()]->fildes[fd]->offset, buf, nbyte);
        kmt->spin_lock(&vfs_lk);
        if(dev_ret > 0) 
            cur_task[_cpu()]->fildes[fd]->offset += dev_ret;
        kmt->spin_unlock(&vfs_lk);
        return dev_ret;
    }

    if(cur_task[_cpu()]->fildes[fd]->offset + nbyte + sizeof(rd_header_t)> RD_BLK_SZ) {
        kmt->spin_lock(&print_lk);
        Log("write too much, exceed one block");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    
    if(cur_task[_cpu()]->fildes[fd]->offset + nbyte + sizeof(rd_header_t)> RD_BLK_SZ) {
        kmt->spin_lock(&print_lk);
        Log("write too much, exceed one block");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
    
    off_t inode_off = cur_task[_cpu()]->fildes[fd]->inode_off;
    dev = cur_task[_cpu()]->fildes[fd]->fs->dev;
    inode_t *inode = pmm->alloc(sizeof(inode_t));
    dev->ops->read(dev, inode_off, inode, sizeof(inode_t));

    off_t write_off = inode->file + sizeof(rd_header_t) + cur_task[_cpu()]->fildes[fd]->offset;
    ssize_t ret = dev->ops->write(dev, write_off, buf, nbyte);
    
    if(ret > 0)
        cur_task[_cpu()]->fildes[fd]->offset += ret;

    if(cur_task[_cpu()]->fildes[fd]->offset > inode->size) {
        inode->size = cur_task[_cpu()]->fildes[fd]->offset;
        dev->ops->write(dev, inode_off, inode, sizeof(inode_t));
    }

    pmm->free(inode);
    kmt->spin_unlock(&vfs_lk);
    return ret;
}

off_t vfs_lseek(int fd, off_t offset, int whence) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("lseek %d, %x, %d", fd, offset, whence);
    kmt->spin_unlock(&print_lk);
    
    if(fd < 0 || cur_task[_cpu()]->fildes[fd] == NULL) {
        kmt->spin_lock(&print_lk);
        Log("illegal fd: %d, lseek failed", fd);
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "devfs", 32) == 0) {
        kmt->spin_unlock(&vfs_lk);
        return -1;
    }
    device_t *dev;
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "devfs", 32) == 0) {
        dev = cur_task[_cpu()]->fildes[fd]->dev;
        if(!dev) {
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        if(whence == SEEK_SET) {
            cur_task[_cpu()]->fildes[fd]->offset = offset;
        }
        else if(whence == SEEK_CUR) {
            cur_task[_cpu()]->fildes[fd]->offset += offset;
        }
        kmt->spin_unlock(&vfs_lk);
        return cur_task[_cpu()]->fildes[fd]->offset;
    }
    
    if(whence == SEEK_SET) {
        if(offset + sizeof(rd_header_t) > RD_BLK_SZ || offset < 0) {
            kmt->spin_lock(&print_lk);
            Log("lseek failed, offset exceed a block or less than 0.");
            kmt->spin_unlock(&print_lk);
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        cur_task[_cpu()]->fildes[fd]->offset = offset; 
    }
    else if(whence == SEEK_CUR) {
        if(cur_task[_cpu()]->fildes[fd]->offset + offset + sizeof(rd_header_t) > RD_BLK_SZ || cur_task[_cpu()]->fildes[fd]->offset + offset < 0) {
            kmt->spin_lock(&print_lk);
            Log("lseek failed, offset exceed a block or less than 0.");
            kmt->spin_unlock(&print_lk);
            
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        cur_task[_cpu()]->fildes[fd]->offset += offset; 
    }
    else if(whence == SEEK_END) {
        off_t inode_off = cur_task[_cpu()]->fildes[fd]->inode_off;
        dev = cur_task[_cpu()]->fildes[fd]->fs->dev;
        inode_t *inode = pmm->alloc(sizeof(inode_t));
        dev->ops->read(dev, inode_off, inode, sizeof(inode_t));
        off_t end_off = inode->size;

        if(end_off + offset + sizeof(rd_header_t) > RD_BLK_SZ || end_off + offset < 0) {
            kmt->spin_lock(&print_lk);
            Log("lseek failed, offset exceed a block or less than 0.");
            kmt->spin_unlock(&print_lk);
            
            kmt->spin_unlock(&vfs_lk);
            return -1;
        }
        cur_task[_cpu()]->fildes[fd]->offset = end_off + offset;
        pmm->free(inode);
    }
    else {
        kmt->spin_lock(&print_lk);
        Log("lseek failed, whence is illegal.");
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return -1;
    } 
    off_t ret = cur_task[_cpu()]->fildes[fd]->offset;
    kmt->spin_unlock(&vfs_lk);
    return ret;
}

int vfs_close(int fd) {
    kmt->spin_lock(&vfs_lk);
    
    kmt->spin_lock(&print_lk);
    Log("close %d", fd);
    kmt->spin_unlock(&print_lk);
    
    if(fd < 0 || cur_task[_cpu()]->fildes[fd] == NULL) {
        kmt->spin_lock(&print_lk);
        Log("illegal fd: %d, close failed", fd);
        kmt->spin_unlock(&print_lk);
        
        kmt->spin_unlock(&vfs_lk);
        return 0;
    }
   
    if(strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "devfs", 32) == 0 || strncmp(cur_task[_cpu()]->fildes[fd]->fs->name, "procfs", 32) == 0) {
        pmm->free(cur_task[_cpu()]->fildes[fd]);
        cur_task[_cpu()]->fildes[fd] = NULL; 
    
        kmt->spin_unlock(&vfs_lk);
        return 1;
    }

    //判断之前有没有unlink过，决定是否删除
    off_t inode_off = cur_task[_cpu()]->fildes[fd]->inode_off;
    device_t *dev = cur_task[_cpu()]->fildes[fd]->fs->dev;
    inode_t *inode = pmm->alloc(sizeof(inode_t));

    dev->ops->read(dev, inode_off, inode, sizeof(inode_t));
    
    inode->refcnt -= 1;
    if(inode->unlink == 1 && inode->linkcnt == 0 && inode->refcnt == 0) {
        kmt->spin_lock(&print_lk);
        Log("unlink");
        kmt->spin_unlock(&print_lk);
        // 清空文件
        char * rm_buf = pmm->alloc(RD_BLK_SZ);
        dev->ops->write(dev, inode->file, rm_buf, RD_BLK_SZ);
        pmm->free(rm_buf);
        // 改inode为无效
        inode->file = -1;
    }
    dev->ops->write(dev, inode_off, inode, sizeof(inode_t));
    

    pmm->free(cur_task[_cpu()]->fildes[fd]);
    cur_task[_cpu()]->fildes[fd] = NULL; 
    
    kmt->spin_unlock(&vfs_lk);
    return 1;
}

MODULE_DEF(vfs) {
    .init = vfs_init,
    .access = vfs_access,
    .mount = vfs_mount,
    .unmount = vfs_unmount,
    .mkdir = vfs_mkdir,
    .rmdir = vfs_rmdir,
    .link = vfs_link,
    .unlink = vfs_unlink,
    .open = vfs_open,
    .read = vfs_read,
    .write = vfs_write,
    .lseek = vfs_lseek,
    .close = vfs_close,
};
