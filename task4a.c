#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s n>=1 m>=100", argv[0]);
    exit(EXIT_FAILURE);
}

void create_players(int n, int m,  pid_t *pids){
    for(int i = 0; i < n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0) {
            printf("%d: I have %d and I'm going to play roulette\n", getpid(), m);
            exit(EXIT_SUCCESS);
        }
        pids[i] = pid;
    }
}

int main(int argc, char **argv){
    if(argc != 3) usage(argc, argv);

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n < 1 || m < 100) usage(argc, argv);

    pid_t *pids = malloc(sizeof(pid_t) * n);
    if(!pids) ERR("malloc");

    create_players(n, m, pids);

    while(wait(NULL) > 0);
    free(pids);
    return EXIT_SUCCESS;
}