#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char **argv){
    fprintf(stderr, "usage %s: 2<=n<=5  5<=m<=10\n", argv[0]);
    exit(EXIT_FAILURE);
}

void player_work(int i, int n, int *to_player, int *from_player){
    //player uzywa: czytanie od serwera i pisanie do serwera
    for(int j = 0; j < n; j++){
        if(j == i){
            //dla swojego zestawu zamykamy nieuzywane konce
            if(close(to_player[2*j + 1]) < 0) ERR("close");
            if(close(from_player[2*j]) < 0) ERR("close");
        } else {
            //cudze pipey sa niepotrzebne
            if(close(to_player[2*j]) < 0) ERR("close");
            if(close(to_player[2*j + 1]) < 0) ERR("close");
            if(close(from_player[2*j]) < 0) ERR("close");
            if(close(from_player[2*j + 1]) < 0) ERR("close"); 
        }
    }
    printf("player %d started: read from fd=%d, wrtie to fd=%d\n", i, to_player[2*i], from_player[2*i+1]);
    exit(EXIT_SUCCESS);
}

void create_players(int n, int *to_player, int *from_player, pid_t *pids){
    for(int i = 0; i < n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            player_work(i, n, to_player, from_player);
        }
        pids[i] = pid; //zapisujemy pid dziecka w rodzicu
    }
}

int main(int argc, char **argv){
    if(argc != 3) usage(argc, argv);
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n < 2 || n > 5 || m < 5 || m > 10) usage(argc, argv);

    int *to_player = malloc(2 * n * sizeof(int)); // server -> player
    int *from_player = malloc(2 * n * sizeof(int)); // player -> server
    pid_t *pids = malloc(n * sizeof(pid_t));

    if(!to_player || !from_player || !pids) ERR("malloc");

    for(int i = 0; i < n; i++){ //tworzymy n pipeow 
        if(pipe(&to_player[2*i]) < 0) ERR("pipe"); //pipe tworzy juz dwa deskryptory wiec tworzymy n pipeow o 2n koncach
        if(pipe(&from_player[2*i]) < 0) ERR("pipe"); //pid kazdego dziecka
    }
    printf("server created %d players\n", n);
    create_players(n, to_player, from_player, pids);

    //serwer uzywa tylko: pisanie do gracza i czytanie od gracza
    for(int i = 0; i < n; i++){
        if(close(to_player[2*i]) < 0) ERR("close");
        if(close(from_player[2*i + 1]) < 0) ERR("close");
    }
//czekamy na dzieci
    while(wait(NULL) > 0);
    if(errno != ECHILD) ERR("wait");
//jak dzieci skonczyly to zamykamy reszte pipow 
    for(int i = 0; i < n; i++){
        if(close(to_player[2*i + 1]) < 0) ERR("close");
        if(close(from_player[2*i]) < 0) ERR("close");
    }

    free(to_player);
    free(from_player);
    free(pids);
    return EXIT_SUCCESS;
}