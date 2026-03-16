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

void player_work(int i, int n, int m, int *to_player, int *from_player){
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
    srand(getpid());
    int cards[m];
    for(int k = 0; k < m; k++){
        cards[k] = k + 1; //m kard 1..M
    }
    int cards_left = m; //ile kart zostalo jeszcze
    for(int r = 0; r < m; r++){
        char new_round[16];
        if(read(to_player[2*i], new_round, 16) < 0) ERR("read");
        int idx = rand() % cards_left; //losujemy indeks z pozostalych kart
        int rand_card = cards[idx]; //karta o tym indeksie

        char message[16]; //bufor na wiadomosc
        memset(message, 0, 16); //najpierw zerujemy cala tablice
        snprintf(message, 16, "%d", rand_card); //zapisujemy liczbe jako tekst, jesli rand_num = 5 to mamy ['5']['\0'][0][0][0][0][0][0][0][0][0][0][0][0][0][0]
            
        if(write(from_player[2*i+1], message, 16) < 0) ERR("write");
        cards[idx] = cards[cards_left - 1]; //czyli np jak mielismy [123456] wylosowalismy 2 to cards[1] = cards[5] -> [163456], cards_left-- -> zatem licza sie teraz tylko pierwsze 5 elementow [162345] 
        cards_left--;
    }
    exit(EXIT_SUCCESS);
}

void create_players(int n, int m, int *to_player, int *from_player, pid_t *pids){
    for(int i = 0; i < n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            player_work(i, n, m, to_player, from_player);
        }
        pids[i] = pid; 
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
    create_players(n, m, to_player, from_player, pids);

    //serwer uzywa tylko: pisanie do gracza i czytanie od gracza
    for(int i = 0; i < n; i++){
        if(close(to_player[2*i]) < 0) ERR("close");
        if(close(from_player[2*i + 1]) < 0) ERR("close");
    }
    for(int r = 0; r < m; r++){
        printf("NEW ROUND\n");
        for(int i = 0; i < n; i++){
            char new_round[16]; 
            memset(new_round, 0, 16);
            snprintf(new_round, 16, "new_round");
            if(write(to_player[2*i + 1], new_round, 16) < 0) ERR("write");
        }
        for(int i = 0; i < n; i++){
            char message[16];
            if(read(from_player[2*i], message, 16) < 0) ERR("read");
            int number = atoi(message);
            printf("got number %d from player %d\n", number, i);
        }
    }

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