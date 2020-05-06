#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h> 
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#define MAX_SO 200

int cnt_name = 0, cnt_so = 0, is_func;
void * handle[MAX_SO];
char wrap_head[30] = "int __expr_wrap_";
char wrap_mid[20] = "() { return (";
char wrap_tail[10] = "); }";
char incl[1000] = {};
int incl_shift = 0;
void expr_into_func(char * new_str, char * str) {
    str[strlen(str) - 1] = '\0';
    snprintf(new_str, 200, "int __expr_wrap_%d(){ return(%s); }", cnt_name, str);
    cnt_name++;
}
int preprocess(char * func_name, char * new_str, char * str) {
    int one_line_func = 0;
    for(int i = strlen(str) - 2; i > 0; i--) {
        if(str[i] == '}') {
            one_line_func = 1;
            break;
        }
    }
    if(str[0] == 'i' && str[1] == 'n' && str[2] == 't' && str[3] == ' ') { 
        one_line_func ++;
    }
    if(one_line_func != 2) {
        // turn a expr into a wrap function
        expr_into_func(new_str, str);
    }
    else {
        strncpy(new_str, str, 200);
    }
    int i = 4;
    while(new_str[i] == ' ')
        i++;
    int cur = 0;
    for (; i < strlen(new_str); i++) {
        if(new_str[i] == ' ' || new_str[i] == '(')
            break;
        func_name[cur] = new_str[i];
        cur++;
    }
    if(one_line_func == 2)
        return 1;
    else 
        return 0;
}
int load(char * func_name, char * new_str) {
    // 函数的定义和加载
 
    char cfile_path[30] = {};
    snprintf(cfile_path, 30, "./temp/%s.c", func_name);

    char sofile_path[30] = {};
    snprintf(sofile_path, 30, "./temp/%s.so", func_name);
    
    FILE * fd = fopen(cfile_path, "w");
    if(fd == NULL) {
        printf("  fopen %s failed.\n", cfile_path);
        assert(0);
    }
    fprintf(fd, "%s%s", incl, new_str);
    
    fclose(fd);
    
    // 生成.so 文件
    pid_t pid = fork();
    if(pid == 0) {
        close(STDERR_FILENO);
        // 根据32位还是64位生成不同的.so文件, 注意这里不-w就会出事
#ifdef __x86_64__
        execlp("/usr/bin/gcc", "gcc", "-Werror=implicit-function-declaration", "-m64", "-fPIC", "-shared", cfile_path, "-o", sofile_path, NULL);
#else
        execlp("/usr/bin/gcc", "gcc", "-Werror=implicit-function-declaration", "-m32", "-fPIC", "-shared", cfile_path, "-o", sofile_path, NULL);
#endif
        printf("  execlp gcc failed.\n");
        assert(0);
    }
    else {
        int status = 0;
        waitpid(pid, &status, 0);
        //printf("gcc status: %d\n", status);
        if(status != 0)
            return 1;
        else if(is_func) {
            int len = (int)strlen(new_str);
            strncat(incl, "extern ", 1000);
            incl_shift += 7;
            for (int i = 0; i < len; i++) {
                incl[incl_shift] = new_str[i];
                incl_shift++;
                if(new_str[i] == ')') {
                    incl[incl_shift] = ';';
                    incl_shift++;
                    incl[incl_shift] = '\n';
                    incl_shift++;
                    break;
                }
            }
            if(incl_shift >= 999) {
                printf("Too much functions\n");
                assert(0);
            }
        }
        handle[cnt_so] = dlopen(sofile_path, RTLD_LAZY | RTLD_GLOBAL);
        if(handle[cnt_so] == NULL) {
            char *error;
            error = dlerror();
            printf("  dlopen failed.\n");
            fprintf(stderr, "%s\n", error);
            assert(0);
        }
        cnt_so ++;
        if(cnt_so >= MAX_SO) {
            printf("Too many .so files");
            assert(0);
        }
    }
    return 0;

}
int main(int argc, char *argv[]) {
    // 创建文件夹
    // 创建一个文件夹最后再删掉吧
    if(mkdir("./temp", S_IRWXU) != 0 && errno != EEXIST) {
        printf("  make dir failed: %d\n", errno);
        assert(0);
    }

    char str[100] = {};
    printf(">> ");
    
    while(fgets(str, 100, stdin) != NULL) {
        char new_str[200] = {};
        char func_name[20] = {};
        if(strncmp(str, "EOF", 3) == 0)
            break;
        is_func = preprocess(func_name, new_str, str);
        
        int err = load(func_name, new_str);
       
        if(err) {
            printf("  Compile Error\n");
            printf(">> ");
            continue;
        }

        if(is_func) {
            printf("  Added: %s", new_str);
            printf(">> ");
        }
        else {
            //printf("%s\n", func_name);
            int (*func)() = dlsym(handle[cnt_so - 1], func_name);
            if(func == NULL) {
                printf("  dlsym failed.\n");
                assert(0);
            }
            int value = func(); // 通过函数指针调用
            printf("  (%s) == %d.\n", str, value);
            printf(">> ");
        }
        
    }
    for (int i = 0; i < cnt_so; i++) {
        dlclose(handle[i]);
    }
    if(system("rm -rf ./temp") != 0) {
        printf("  rmdir failed.\n");
    }
    return 0;
}
