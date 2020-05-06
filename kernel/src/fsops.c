#include "common.h"

extern spinlock_t print_lk;

char new_buf[RD_BLK_SZ];
char inode_buf[RD_BLK_SZ];
char file_buf[RD_BLK_SZ];

off_t find_free_rdblk(device_t *dev, rd_header_t * prev) {
    rd_t * rd = (rd_t *)dev->ptr;
    off_t off = 0;
    while(off + (off_t)rd->start < (off_t)rd->end) {
        char temp[64];
        dev->ops->read(dev, off, (void *)temp, sizeof(rd_header_t));
        rd_header_t *header = (rd_header_t *)temp;
        if(!header->occupied) {
            header->occupied = 1;
            header->next = -1;
            header->free_start = sizeof(rd_header_t);
            if(prev)
                prev->next = off;
            dev->ops->write(dev, off, (void *)temp, sizeof(rd_header_t));
            //Log("find free blk: %x", off);
            return off;
        }
        off += RD_BLK_SZ;
    }
    printf("no free Ramdisk sapce.\n");
    assert(0);
    return 0;
}

void fs_init(struct filesystem *fs, const char *name, device_t *dev) {
    fs->dev = dev;
    fs->name = name;
    off_t off = find_free_rdblk(dev, NULL);
    fs->root_header = off;
    dev->ops->read(dev, off, inode_buf, sizeof(rd_header_t));
    rd_header_t *header = (rd_header_t *)inode_buf;
    inode_t *new_inode = (inode_t *)(inode_buf + header->free_start);
    new_inode->inode_off = off + sizeof(rd_header_t);
    new_inode->size = sizeof(dir_t);
    header->free_start += sizeof(inode_t);
    
    // inode_t *new_inode = (inode_t *)(pmm->alloc(sizeof(inode_t)));
    // void *new_ptr = (pmm->alloc(4096));
    off_t dir_off = find_free_rdblk(dev, NULL);
    //Log("root: %x, dir: %x", off, dir_off); 
    new_inode->file = dir_off;
    new_inode->unlink = 0;
    new_inode->refcnt = 0;
    new_inode->linkcnt = 0;
    new_inode->ops = &inode_ops;
    new_inode->fs = fs;
    new_inode->type = DIR;
    dev->ops->write(dev, off, inode_buf, sizeof(rd_header_t) + sizeof(inode_t));// 写入块的header和第一个inode
    
    //Log("write %x", off);
    
    dev->ops->read(dev, dir_off, new_buf, sizeof(rd_header_t));
    rd_header_t *dir_header = (rd_header_t *)new_buf;
    dir_t *new_dir = (dir_t *)(new_buf + dir_header->free_start);
    dir_header->free_start += sizeof(dir_t);
    new_dir->son_inode[0] = 0;
    strncpy(new_dir->name, "/", 2);
    dev->ops->write(dev, dir_off, new_buf, sizeof(rd_header_t) + sizeof(dir_t));//写入一个header和一个目录文件
    //Log("write %x", dir_off);
    //printf("new filesystem %s\n", name);
}

// 根据path找到fs文件系统中的inode
off_t fs_lookup(struct filesystem *fs, const char *path, int flags) {
    device_t *dev = fs->dev; 
    dev->ops->read(dev, fs->root_header, inode_buf, RD_BLK_SZ);
    //Log("read %x", fs->root_header);
    rd_header_t * root_header = (rd_header_t *)inode_buf;
    inode_t *cur_inode = (inode_t *)(inode_buf + sizeof(rd_header_t));
    dev->ops->read(dev, cur_inode->file, file_buf, RD_BLK_SZ);
    off_t dir_off = cur_inode->file;
    //Log("read %x", cur_inode->file);
    //rd_header_t * dir_header = (rd_header_t *)file_buf;
    dir_t *cur_dir = (dir_t *)(file_buf + sizeof(rd_header_t));

    //Log("root: %x, file: %x, dir: %s\n", fs->root_header, cur_inode->file, cur_dir->name); 
    
    //Log("look up %s\n", path);
    if(path[0] != '/') {
        kmt->spin_lock(&print_lk);
        Log("path not started with /.");
        kmt->spin_unlock(&print_lk);
        return 0;
    }
    if(strlen(path) == 1) {
        //inode_t * ret_inode = pmm->alloc(sizeof(inode_t));
        //memcpy(ret_inode, cur_inode, sizeof(inode_t));
        return cur_inode->inode_off;
    }
    path = path + 1;
    char temp[32];
    int is_end = 0;
    while(1) {
        memset(temp, 0, sizeof(temp));
        int len = strlen(path);
        int if_find = 0;
        for (int i = 0; i < len; i++) {
            if(path[i] == '/') {
                path = path + i + 1;
                break;
            }
            temp[i] = path[i];
            if(i == len - 1) {
                path = path + i + 1;
                is_end = 1;
            }
        }
        //printf("look for %s\n", temp);
        if(cur_dir == NULL) {
            assert(0);
        }

        kmt->spin_lock(&print_lk);
        Log("Dir %s: ", cur_dir->name);
        for (int i = 0; i < 64 && cur_dir->son_inode[i]; i++) {
            if(cur_dir->son_inode[i] == -1)
                continue;//已经删除的
            inode_t *son = (inode_t *)(inode_buf + cur_dir->son_inode[i]);
            if(son->file == -1) 
                continue;
            Log("%s ", cur_dir->son_name[i]);
        }
        kmt->spin_unlock(&print_lk);

        for (int i = 0; i < 64 && cur_dir->son_inode[i]; i++) {
            if(cur_dir->son_inode[i] == -1)
                continue;//已经删除的
            
            inode_t *son = (inode_t *)(inode_buf + cur_dir->son_inode[i]);
            if(son->file == -1) {
                //之前unlink过的
                cur_dir->son_inode[i] = -1;
                dev->ops->write(dev, dir_off, file_buf,  sizeof(rd_header_t) + sizeof(dir_t));
                continue;
            }
            if(strncmp(temp, cur_dir->son_name[i], 32) == 0) {
                //Log("son: %x, %d", cur_dir->son_inode[i], (cur_dir->son_inode[i] == -1));
                if(son->type == FILE && (flags & FLAG_DIR)) {
                    kmt->spin_lock(&print_lk);
                    Log("%s is a directory, not a file.", temp);
                    kmt->spin_unlock(&print_lk);
                    return 0;
                }
                if(is_end && son->type == DIR && !(flags & FLAG_DIR)) {
                    kmt->spin_lock(&print_lk);
                    Log("%s is a directory, not a file.", temp);
                    kmt->spin_unlock(&print_lk);
                    return 0;
                }
                if(!is_end && son->type == FILE) {
                    kmt->spin_lock(&print_lk);
                    Log("%s is a file, not a directory.", temp);
                    kmt->spin_unlock(&print_lk);
                    return 0;
                }
                
                if(is_end && (flags & FLAG_DELETE)) {
                    kmt->spin_lock(&print_lk);
                    Log("delete %s", temp);
                    kmt->spin_unlock(&print_lk);
                    inode_t * rm_inode = (inode_t *)(inode_buf + cur_dir->son_inode[i]);
                    off_t rm_off = rm_inode->file;
                    //检查要删除的文件夹是否为空
                    if(rm_inode->type == DIR) {
                        dev->ops->read(dev, rm_off, new_buf,  sizeof(rd_header_t) + sizeof(dir_t));
                        dir_t * rm_dir = (dir_t *)(new_buf + sizeof(rd_header_t));
                        for (int i = 0; i < 64 && cur_dir->son_inode[i]; i++) {
                            if(rm_dir->son_inode[i] > 0) {
                                kmt->spin_lock(&print_lk);
                                Log("Directory not empty. rmdir failed");
                                kmt->spin_unlock(&print_lk);
                                return 0;
                            }
                        }
                    }
                    //Log("delete from %x", rm_off);
                    //Log("father %x", dir_off);
                    cur_dir->son_inode[i] = -1;//删除
                    dev->ops->write(dev, dir_off, file_buf,  sizeof(rd_header_t) + sizeof(dir_t));
                    memset(file_buf, 0, RD_BLK_SZ);
                    dev->ops->write(dev, rm_off, file_buf, RD_BLK_SZ);
                    return 1; 
                }

                if_find = 1;
                dir_off = son->file;
                cur_inode = son;
                if(!is_end) {
                    dev->ops->read(dev, dir_off, file_buf,  sizeof(rd_header_t) + sizeof(dir_t));
                    //rd_header_t *son_header = (rd_header_t *)(file_buf);
                    cur_dir = (dir_t *)(file_buf + sizeof(rd_header_t));
                }
                break;
            }
        }
        if(!if_find) {
            //printf("cannot find %s\n", temp);
            break;
        }
        if(is_end) {
            //printf("find %s\n", temp);
            //inode_t * ret_inode = pmm->alloc(sizeof(inode_t));
            //memcpy(ret_inode, cur_inode, sizeof(inode_t));
            if(flags & FLAG_ADD_REF) {
                cur_inode->refcnt ++;
                dev->ops->write(dev, cur_inode->inode_off, cur_inode, sizeof(inode_t));
            }
            //Log("inode off %x, refcnt %d", cur_inode->inode_off, cur_inode->refcnt);
            return cur_inode->inode_off;
            //return ret_inode;
        }
    }

    if((flags & FLAG_CREATE) == 0) {
        kmt->spin_lock(&print_lk);
        Log("no such file");
        kmt->spin_unlock(&print_lk);
        return 0;
    }
    
    if(strlen(path) != 0) {
        kmt->spin_lock(&print_lk);
        Log("the path is ilegal, cannot create.");
        kmt->spin_unlock(&print_lk);
        return 0;
    }

    //Log("create %s", temp);
    if(cur_dir == NULL) {
        assert(0);
    }
    inode_t *new_inode = (inode_t *)(inode_buf + root_header->free_start);
    new_inode->inode_off = root_header->free_start + fs->root_header;
    off_t new_off = find_free_rdblk(dev, NULL);  
    dev->ops->read(dev, new_off, new_buf, sizeof(rd_header_t));
    //rd_header_t *new_header = (rd_header_t *)new_buf; 
    new_inode->file = new_off;
    new_inode->unlink = 0;
    if(flags & FLAG_ADD_REF)
        new_inode->refcnt = 1;
    else
        new_inode->refcnt = 0;
    new_inode->linkcnt = 1;
    //Log("new inode refcnt %d", new_inode->refcnt);
    new_inode->ops = &inode_ops;
    new_inode->fs = fs;
    if(flags & FLAG_DIR) {
        new_inode->size = sizeof(dir_t);
        new_inode->type = DIR;
    } 
    else {
        new_inode->size = 0;
        new_inode->type = FILE;
    }

    int i;
    for (i = 0; i < 64 && cur_dir->son_inode[i]; i++);
    cur_dir->son_inode[i] = root_header->free_start;
    //Log("inode start from %x", root_header->free_start);
    root_header->free_start += sizeof(inode_t);
    if((int)(root_header->free_start) > 4000) {
        printf("too much inodes");
        assert(0);
    }
    dev->ops->write(dev, fs->root_header, inode_buf, RD_BLK_SZ);
    //Log("write %x", fs->root_header);
    strncpy(cur_dir->son_name[i], temp, 32);
    dev->ops->write(dev, dir_off, file_buf, sizeof(rd_header_t) + sizeof(dir_t));

    //Log("%s is son %d of %s", cur_dir->son_name[i], i, cur_dir->name);
    if(flags & FLAG_DIR) {
        dir_t *new_dir = (dir_t *)(new_buf + sizeof(rd_header_t));
        new_dir->son_inode[0] = 0;
        strncpy(new_dir->name, temp, 32);
        dev->ops->write(dev, new_off, new_buf, sizeof(rd_header_t) + sizeof(dir_t));
        //cur_dir = new_dir;
        //Log("new dir %s from %x", cur_dir->name, new_off);
        //memcpy(file_buf, new_buf, sizeof(rd_header_t) + sizeof(dir_t));
    }
    else {
        //printf("new file %s\n", temp);
        dev->ops->write(dev, new_off, new_buf, sizeof(rd_header_t));
        //Log("write %x", new_off);
        //Log("new file %s from %x", temp, new_off);
    }
    
    //inode_t * ret_inode = pmm->alloc(sizeof(inode_t));
    //memcpy(ret_inode, new_inode, sizeof(inode_t));
    //Log("new inode off %x", new_inode->inode_off);
    return new_inode->inode_off;
}

int fs_close(inode_t *inode) { 
    //off_t off = inode->inode_off;
    //device_t * dev = inode->fs->dev;
    //dev->ops->write(dev, off, inode, sizeof(inode));
    TODO();
    return 0;
}

fsops_t blk_fs_ops = {
    .init = fs_init,
    .lookup = fs_lookup,
    .close = fs_close,
};

void dev_fs_init(struct filesystem *fs, const char *name, device_t *dev) {
    fs->dev = dev;
    fs->name = name;
}

fsops_t dev_fs_ops = {
    .init = dev_fs_init,
    .lookup = fs_lookup,
    .close = NULL,
};
