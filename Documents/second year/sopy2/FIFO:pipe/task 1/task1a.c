#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void create_children(int n, int *R1, int *R2, int *R3){
    for(int i = 2; i < 2 + n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            if(i == 2){
                printf("process 2 pid: %d, uses: R1[0] = %d and R2[1] = %d\n", getpid(), R1[0], R2[1]);
                printf("process 2 is closing R1[1] = %d, R2[0] = %d, R3[0] = %d, R3[1] = %d\n", R1[1], R2[0], R3[0], R3[1]);
                if(close(R1[1]) < 0) ERR("close");
                if(close(R2[0]) < 0) ERR("close");
                if(close(R3[0]) < 0) ERR("close");
                if(close(R3[1]) < 0) ERR("close");
            } else if (i == 3){
                printf("process 3 pid: %d, uses: R2[0] = %d and R3[1] = %d\n", getpid(), R2[0], R3[1]);
                printf("process 3 is closing R1[0] = %d, R1[1] = %d, R2[1] = %d, R3[0] = %d\n", R1[0], R1[1], R2[1], R3[0]);
                if(close(R1[0]) < 0) ERR("close");
                if(close(R1[1]) < 0) ERR("close");
                if(close(R2[1]) < 0) ERR("close");
                if(close(R3[0]) < 0) ERR("close");
            }
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char **argv){
    //parent pisze do procesu 2 i czyta z procesu 3 -> R1[1], R3[0]
    //proces 2 czyta od parenta i pisze do procesu 3 -> R1[0], R2[1]
    //proces 3 czyta z procesu 2 i pisze do parenta -> R2[0], R3[1]
    int R1[2]; //parent -> process2
    if(pipe(R1) < 0) ERR("pipe");
    int R2[2]; //process2 -> process3
    if(pipe(R2) < 0) ERR("pipe");
    int R3[2]; //process3 i parent
    if(pipe(R3) < 0) ERR("pipe");

    printf("parent process pid: %d, uses: R1[1] = %d and R3[0] = %d\n", getpid(), R1[1], R3[0]);
    create_children(2, R1, R2, R3);
    printf("parent is closing R1[0] = %d, R2[0] = %d, R2[1] = %d, R3[1] = %d\n", R1[0], R2[0], R2[1], R3[1]);

    if(close(R1[0]) < 0) ERR("close");
    if(close(R2[0]) < 0) ERR("close");
    if(close(R2[1]) < 0) ERR("close");
    if(close(R3[1]) < 0) ERR("close");

    while(wait(NULL) > 0);
    if(errno != ECHILD) ERR("wait"); //gdy nie ma dzieci wait() ustawia errno = ECHILD 
    return EXIT_SUCCESS;
}