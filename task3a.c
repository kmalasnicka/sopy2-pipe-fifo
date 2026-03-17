#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s 3<=n<=20", argv[0]);
    exit(EXIT_FAILURE);
}

void create_students(int n){
    for(int i = 0; i < n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            printf("Student: %d\n", getpid());
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char **argv){
    if(argc != 2) usage(argc, argv);
    int n = atoi(argv[1]);
    if(n < 3 || n > 20) usage(argc, argv);
    create_students(n);
    while(wait(NULL) > 0);
    return EXIT_SUCCESS;
}