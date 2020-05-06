#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct tagBPB {
    uint64_t dummy0;//0-7
    uint16_t dummy1;//8-9
    uint8_t dummy2;//A
    uint16_t BytePerSector;//B-C
    uint8_t SectorsPerCluster;//D
    uint16_t ReservedSector;//E-F
    uint8_t NumberOfFAT;//10
    uint16_t RootEntries;//11-12
    uint16_t SmallSector;//13-14
    uint8_t MediaDescriptor;//15
    uint8_t ignore[14];//16-23
    uint32_t SectorsPerFAT;//24-27
    uint8_t ignore1[4];//28-2B
    uint32_t RootClusterNumber;//2C-2F
    //...

}__attribute__((packed)) BPB;

typedef struct tagLONGNAME {
    uint8_t property;
    char name1[10];
    uint8_t LongMark;
    uint8_t reserve;
    uint8_t check;
    char name2[12];
    uint16_t FirstCluster;
    char name3[4];
}__attribute__((packed)) longname;

typedef struct tagDTB {
    char name[8];
    char type[3];
    uint8_t ignore1[9];
    uint16_t FirstClusterHigh;
    uint8_t ignore2[4];
    uint16_t FirstClusterLow;
    uint32_t Length;
}__attribute__((packed)) DTB;

typedef struct tagBITMAPFILEHEADER {
    uint16_t bfType;//位图文件的类型，必须为BM(1-2字节）
    uint32_t bfSize;//位图文件的大小，以字节为单位（3-6字节，低位在前）
    uint16_t bfReserved1;//位图文件保留字，必须为0(7-8字节）
    uint16_t bfReserved2;//位图文件保留字，必须为0(9-10字节）
    uint32_t bfOffBits;//位图数据的起始位置，以相对于位图（11-14字节，低位在前）
    //文件头的偏移量表示，以字节为单位
}__attribute__((packed)) bmpheader;

void mypanic(char* log) {
    printf("%s\n", log);
    assert(0);
}

uintptr_t filestart;
uintptr_t filesz;
uintptr_t clusterstart;
BPB* bpb;

void read_file(char *filename) {
    int fd = open(filename, O_RDONLY);
    if(!fd) 
        mypanic("read file failed");
    struct stat statbuf;
    stat(filename, &statbuf);
    filesz = (uintptr_t)statbuf.st_size;
    filestart = (uintptr_t)mmap(NULL, filesz, PROT_READ, MAP_PRIVATE, fd, 0);
    bpb = (BPB*)filestart;
    //printf("Byte Per Sector: 0x%x\n", bpb->BytePerSector);
    //printf("Sectors Per Cluster: 0x%x\n", bpb->SectorsPerCluster);
    uintptr_t FAT1 = bpb->ReservedSector * bpb->BytePerSector;
    //DBR所在位置0+保留扇区数*扇区大小
    //printf("FAT1 start:0x%x\n", (uint32_t)FAT1);
    //printf("SectorsPerFAT start:0x%x\n", (uint32_t)bpb->SectorsPerFAT);
    //printf("BytePerSector start:0x%x\n", (uint32_t)bpb->BytePerSector);
    clusterstart = FAT1 + bpb->BytePerSector * bpb->SectorsPerFAT * bpb->NumberOfFAT;
    //FAT1+FAT扇区数*扇区大小*FAT数
    // printf("cluster start:0x%x\n", (uint32_t)clusterstart);
    clusterstart -= bpb->BytePerSector * bpb->SectorsPerCluster * bpb->RootClusterNumber;
    // printf("cluster start:0x%x\n", (uint32_t)clusterstart);
}

void recover() {
    int count = 0;
    for (uintptr_t i = filestart; (uintptr_t)i < filesz + filestart; i += 16) {
        DTB *dtb = (DTB*)i;
        if(strncmp(dtb->type, "BMP", 3) == 0) {
            //printf("start = %x\n", (int)(i - filestart));
            if((unsigned char)dtb->name[0] == 0xe5)
                continue;// 删除了的
            int lflag = 0;
            int cnt = 0;
            char name[100] = {};
            if(i - filestart >= 32) {
                uintptr_t j = i - 32;
                longname *lname = (longname*)j;
                int end = 0;
                while(lname->LongMark == 0x0F) {
                    lflag = 1;
                    for (int i = 0; i < 10; i++) {
                        if(!lname->name1[i] || lname->name1[i] == -1)
                            continue;
                        name[cnt] = lname->name1[i];
                        cnt++;
                    }
                    for (int i = 0; i < 12; i++) {
                        if(!lname->name2[i] || lname->name2[i] == -1)
                            continue;
                        name[cnt] = lname->name2[i];
                        cnt++;
                    }
                    for (int i = 0; i < 4; i++) {
                        if(!lname->name3[i] || lname->name3[i] == -1)
                            continue;
                        name[cnt] = lname->name3[i];
                        cnt++;
                    }

                    end = (lname->property & 0x40);
                    //printf("no: %d\n", (lname->property & 0x1f));
                    //if(lname->property & 0x40) {
                    //    printf("end\n");
                    //}
                    j -= 32;
                    if(j < filestart)
                        break;
                    lname = (longname*)j;
                }
                if(end && strlen(name)) {
                    //printf("[%d] %s\n", count, name);
                    uintptr_t FirstCluster = (dtb->FirstClusterHigh << 16) + dtb->FirstClusterLow;
                    uintptr_t bmpstart = filestart + clusterstart + FirstCluster * bpb->BytePerSector * bpb->SectorsPerCluster;
                    
                    char path[100] = {};
                    sprintf(path, "%s", name);
                    //sprintf(path, "./figs/%s", name);
                    FILE *fp = fopen(path, "w+");
                    fwrite((void*)bmpstart, 1, dtb->Length, fp);
                    char command[100] = {};
                    sprintf(command, "sha1sum %s", path);
                    fflush(fp);
		            fclose(fp); 
                    //char test[100] = {};
                    //sprintf(test, "diff img/%s %s", path, path);
                    //printf("%s\n", test);
                    //system(test);
                    
                    system(command);
                    unlink(path);
                    //fclose(fp); 
                
                    count++;
                    //printf("Cluster: 0x%x\n", (dtb->FirstClusterHigh << 16) + dtb->FirstClusterLow);
                    //printf("filesize: 0x%x\n", (dtb->Length));
                    //printf("bmp offet:0x%x\n", (uint32_t)(bmpstart - filestart));
                    //bmpheader *bmph =(bmpheader *)bmpstart;
                    //printf("bmp size: %x, bmp offset:%x\n", bmph->bfSize, bmph->bfOffBits);
                    //uint8_t R0, G0, B0, R1, G1, B1; 
                    //R0 = *(uint8_t*)(bmpstart + bmph->bfOffBits);
                    //B0 = *(uint8_t*)(bmpstart + bmph->bfOffBits + 1);
                    //G0 = *(uint8_t*)(bmpstart + bmph->bfOffBits + 2);
                    //R1 = *(uint8_t*)(bmpstart + bmph->bfOffBits + 3);
                    //B1 = *(uint8_t*)(bmpstart + bmph->bfOffBits + 4);
                    //G1 = *(uint8_t*)(bmpstart + bmph->bfOffBits + 5);
                    //printf("%x %x %x, %x %x %x\n", R0, G0, B0, R1, G1, B1);
                }
            }
            if(!lflag) {
                //printf("[%d] short name %s\n", count, dtb->name);
                //printf("start 0x%x\n", (int)(i - filestart));
                //count++;
            }
        }
    }
    //printf("tot: %d\n", count);
}

int main(int argc, char *argv[]) {
    //printf("%d\n", argc);
    //for (int i = 0; i < argc; i++) {
    //    printf("%s\n", argv[i]);
    //}
    read_file((char*)argv[1]);
    recover();
    return 0;
}
