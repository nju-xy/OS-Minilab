#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h> // for bool type, in C99
#include <bits/types.h> //pid_t...
#include <dirent.h>
#include <stdlib.h> //qsort

bool if_n = 0, if_p = 0, if_V = 0;
char pid[500000][10];
struct PROC{
    int PPid;
    char Name[105];
    int son[100];
    int num_of_son;
}proc[500000];
int line[100] = {};
int cnt = 0;
void get_info();
void print_tree(int pid, int retract, bool first_son, bool last_son);
void print_version();

int main(int argc, char *argv[]) {
  printf("Hello, World!\n");
  int i;
  for (i = 0; i < argc; i++) {
    assert(argv[i]); // always true
    printf("argv[%d] = %s\n", i, argv[i]);
    
    //得到命令行的参数，根据要求设置标志变量的数值；
    if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--show-pids") == 0)
        if_p = 1;
    if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric-sort") == 0)
        if_n = 1;
    if(strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0)
        if_V = 1;
  }
  assert(!argv[argc]); // always true
  
  get_info();

  //根结点进程号为1，不缩进，是第一个也是最后一个儿子
  if(!if_V) print_tree(1, 0, 1, 1);
  else print_version();
  return 0;
}

void get_info(){
    DIR * dir;//文件夹
    struct dirent * ptr;//目录文件
    dir = opendir("/proc");//打开文件夹/proc
    //assert(dir);
    if(dir == NULL) {
        printf("/proc not found!\n");
        return;
    }
    while((ptr = readdir(dir)) != NULL)
    {
        //get pid
        if(ptr->d_name[0] < '0' || ptr->d_name[0] > '9')
            continue;
        //printf("d_name : %s\n", ptr->d_name);//输出文件名             
        int Pid;
        sscanf(ptr->d_name, "%d", &Pid);
        
        //get name and ppid
        char filename[100] = "/proc/";
        strcat(filename, ptr->d_name);
        strcat(filename, "/status");
        
        FILE *fp = fopen(filename, "r");
        //assert(fp);
        if(fp == NULL) {
            printf("%s not found!\n", filename);
            continue;
        }
        fscanf(fp, "%*s");
        fscanf(fp, "%s", proc[Pid].Name);
        //Name永远为第二个字符串，因此丢弃第一个，取第二个即可
        //这里假设进程名字中没有空格！！！

        //寻找PPid
        char search[100] = {};
        while(strcmp(search, "PPid:") != 0)
            fscanf(fp, "%s", search);
        fscanf(fp, "%d", &proc[Pid].PPid); 
        
        //记录结点的子结点
        int ppid = proc[Pid].PPid;
        proc[ppid].son[proc[ppid].num_of_son] = Pid;
        proc[ppid].num_of_son ++;

        //寻找task中记录的子结点
        char dirname[100] = "/proc/";
        strcat(dirname, ptr->d_name);
        strcat(dirname, "/task");
        
        DIR * dir2;//文件夹
        struct dirent * ptr2;//目录文件
        dir2 = opendir(dirname);//打开文件夹/proc
        //assert(dir2);
        if(dir2 == NULL) {
            printf("%s not found!\n", dirname);
            continue;
        }
        while((ptr2 = readdir(dir2)) != NULL)
        {
            if(ptr2->d_name[0] < '0' || ptr2->d_name[0] > '9') continue;
            int taskid;
            sscanf(ptr2->d_name, "%d", &taskid);
            if(taskid == Pid) continue;
            proc[Pid].son[proc[Pid].num_of_son] = taskid;
            proc[Pid].num_of_son ++;
            strcpy(proc[taskid].Name, "{");
            strcat(proc[taskid].Name, proc[Pid].Name);
            strcat(proc[taskid].Name, "}");
            // printf("%d %d %s\n", taskid, Pid, proc[Pid].Name);
            // snprintf(proc[taskid].Name, 100, "{%s}", proc[Pid].Name);
        }
        closedir(dir2);
        fclose(fp);
    }
    closedir(dir);
}

int len(int num){
    if(num == 0) return 1;
    int ret = 0;
    while(num != 0) {
        ret++; num /= 10;
    }
    return ret;
}

int cmp1(const void *a, const void *b )
{
    return *(int *)a - *(int *)b;
}

int cmp2(const void *a, const void *b )
{
    int A = *(int *)a; int B = *(int *)b;
    return strcmp(proc[A].Name, proc[B].Name);
}

void print_tree(int pid, int retract, bool first_son, bool last_son){
    if(pid != 1) {
        if(first_son && last_son)
            printf("───");
        else if(first_son) {
            printf("─┬─");
            cnt ++;
            line[cnt] = retract + 1;
        }
        else{
            int j = 1;
            for(int i = 0; i < retract; ++i) {
                if(i == line[j] && i != 0) {
                    printf("│");
                    j ++;
                }
                else printf(" ");
            }
            if(last_son) {
                printf(" └─");
                assert(cnt >= 0);
                line[cnt] = 0;
                cnt --;
            }
            else printf(" ├─");
        }
    }
    printf("%s", proc[pid].Name);
    if(if_p) printf("(%d)", pid);
    int new_retract = retract;
    new_retract += strlen(proc[pid].Name);
    if(pid != 1) new_retract += 3;
    if(if_p) new_retract += 2 + len(pid);

    if(proc[pid].num_of_son == 0)  printf("\n");
    
    if(if_n) 
        qsort(proc[pid].son, proc[pid].num_of_son, sizeof(int), cmp1);//按Pid排序
    else 
        qsort(proc[pid].son, proc[pid].num_of_son, sizeof(int), cmp2);//按字母序
    
    for(int i = 0; i < proc[pid].num_of_son; ++i) {
        if(i == 0 && proc[pid].num_of_son == 1)
            print_tree(proc[pid].son[i], new_retract, 1, 1);
        else if(i == 0) 
            print_tree(proc[pid].son[i], new_retract, 1, 0);
        else if(i == proc[pid].num_of_son - 1) 
            print_tree(proc[pid].son[i], new_retract, 0, 1);
        else
            print_tree(proc[pid].son[i], new_retract, 0, 0);
    }
}

void print_version(){
    printf("pstree 1.0   !!!(^_^)!!!\nCopyright Xie Yi\n");
}
