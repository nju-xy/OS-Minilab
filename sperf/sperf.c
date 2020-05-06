#include <stdio.h>
#include <regex.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#define MAXN 500
char str[1000];
char str2[500];
double sum = 0;
double T[MAXN];
char syscall_str[MAXN][20] = {};
int cnt = 0;

void output() {
    // count and output
    printf("\033c");
    /* for (int i = 0; i < cnt; i++) {
        for (int j = 0; j < i; j++) {
            if(strcmp(syscall_str[i], syscall_str[j]) != 0)
                continue;
            Time[j] += Time[i];
            Time[i] = 0;
        }
    }*/
        
    for (int i = 0; i < cnt; i++) {
        // printf("%s %lf\n", syscall_str[i], T[i]);
        sum += T[i];
    }
    for (int i = 0; i < cnt; i++) {
        if(sum == 0)
            break;
        if(strlen(syscall_str[i]) < 25) {
            printf("%s", syscall_str[i]);
            for (int j = 0; j < 25 - strlen(syscall_str[i]); j++) {
                printf(" ");
            }
            printf("%%%.2lf\n", 100.0 * T[i] / sum);
            T[i] = 0;
        }
    }
    // printf("sum = %lf, cnt = %d\n", sum, cnt);
    sum = 0;
    cnt = 0;
    // memset(syscall_str, 0, sizeof(syscall_str));
    // memset(str2, 0, sizeof(str2));
}

int main(int argc, char *argv[], char * envp[]) {
    
    int pipe_id[2];
    if (pipe(pipe_id) != 0) {
          // 出错处理
          assert(0);
    }
    pid_t pid = fork();
    if(pid == 0) {
        //子进程，执行strace命令
        // close(STDOUT_FILENO);
        int fd_null = open("/dev/null", O_WRONLY);
        dup2(fd_null, STDOUT_FILENO);

        close(pipe_id[0]);// close read
        dup2(pipe_id[1], STDERR_FILENO);// connect stderr to pipe write
        close(pipe_id[1]);
        // close(STDERR_FILENO);
        char * argv_new[100] = {"strace", "-T", "-xx", NULL};
        for (int i = 1; ; i++) {
            argv_new[i + 2] = argv[i];
            if(argv[i] == NULL)
                break;
        }
        
        //execvp("/usr/bin/strace", argv_new);
        execv("/usr/bin/strace", argv_new);
        assert(0);
    }
    else {
        //父进程，读取strace输出并统计
        //close(STDOUT_FILENO);
        //close(STDERR_FILENO);
        //int status = 0;
        //waitpid(pid, &status, 0);
        dup2(pipe_id[0], STDIN_FILENO);// connect stdin to pipe read
        close(pipe_id[1]);
        close(pipe_id[0]);
        // close(STDOUT_FILENO);
        //printf("start\n");
        
        char reg_expr1[100] = "<.*>";
        regex_t reg1;
        regmatch_t mat1[1];
        regcomp (&reg1, reg_expr1, 0);
        
        time_t raw_time = time(0);

        while(fgets(str, 500, stdin) > 0) {
            // printf("%d\n", (int)strlen(str));
            // printf("%s\n", str);
            if(strncmp(str, "+++ exited with", 15) == 0)
                break;
            
            if(!(str[0] <= 'z' && str[0] >= 'a') && !(str[0] <= 'Z' && str[0] >= 'A')) {
                if(fgets(str, 500, stdin) <= 0)
                    break;
                if(strncmp(str, "+++ exited with", 15) == 0)
                    break;
               //  printf("%s\n", str);
            }
            if(str[strlen(str) - 2] != '>') {
                fgets(str2, 500, stdin);
                if(strncmp(str2, "+++ exited with", 15) == 0) {
                    //printf("%s\n", str2);
                    break;
                }
                strncat(str, str2, 1000);
            }
            //printf("%s\n", str);
            if(!(str[0] <= 'z' && str[0] >= 'a'))
                continue;
            //find the syscall
            char temp[20] = {};
            int cur2;
            for (cur2 = 0; str[cur2] && str[cur2] != '('; cur2++) {
                temp[cur2] = str[cur2];
                //syscall_str[cnt][cur2] = str[cur2];
            }


            //find the time
            regexec(&reg1, str, 20, mat1, 0);
            char time_str[20];
            int cur1 = 0;
            double t;
            for (int i = mat1[0].rm_so + 1; i < mat1[0].rm_eo - 1; i++) {
                time_str[cur1] = str[i];
                cur1++;
            }
            time_str[cur1] = '\0';
            sscanf(time_str, "%lf", &t);
            
            //printf("%d: %s  ", cnt, syscall_str[cnt]);
            //printf("%lf\n", time[cnt]);
            
            // sum += t;
            
            int flag = 0;
            syscall_str[cnt][cur2] = '\0';
            for (int i = 0; i < cnt; i++) {
                if(strcmp(syscall_str[i], temp) == 0) {
                    T[i] += t;
                    flag = 1;
                    break;
                }
            }


            if(flag == 0){
                T[cnt] = t;
                strncpy(syscall_str[cnt], temp, 20);
                cnt ++;
                assert(cnt < MAXN);
            }

            time_t cur_time = time(0);
            if(cur_time - raw_time >= 1) {
                output();
                raw_time = cur_time;
            }            
        }
        
        // printf("%d different syscall(s), total time is %lf\n", cnt, sum);
        output();
    }
    return 0;
}
