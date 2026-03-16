#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

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
    int cards[m]; //tablica trzymajaca karty ktore gracz ma w danym momencie
    for(int k = 0; k < m; k++){
        cards[k] = k + 1; //m kart 1..M
    }
    int cards_left = m; //ile kart zostalo jeszcze
    for(int r = 0; r < m; r++){
        char new_round[16];
        ssize_t s = read(to_player[2*i], new_round, 16); //sygnal poczatku rundy
        if(s < 0) ERR("read");
        if(s == 0) exit(EXIT_SUCCESS);
        
//po starcie rundy jest 5% szans ze gracz umrze
        if((rand() % 100) < 5) exit(EXIT_SUCCESS);
        int idx = rand() % cards_left; //losujemy indeks z dostepnych kart
        int rand_card = cards[idx]; //karta o tym indeksie
//wysylanie karty do serwera
        char message[16]; //bufor na wiadomosc
        memset(message, 0, 16); //najpierw zerujemy cala tablice
        snprintf(message, 16, "%d", rand_card); //zapisujemy liczbe jako tekst, jesli rand_num = 5 to mamy ['5']['\0'][0][0][0][0][0][0][0][0][0][0][0][0][0][0]   
        if(write(from_player[2*i+1], message, 16) < 0) ERR("write");
//usuwamy karte z zestawu kart
        cards[idx] = cards[cards_left - 1]; //czyli np jak mielismy [123456] wylosowalismy 2 to cards[1] = cards[5] -> [163456], cards_left-- -> zatem licza sie teraz tylko pierwsze 5 elementow [162345] 
        cards_left--;
//po zagraniu gracz czeka na wiadomosc serwera
        char points[16];
        s = read(to_player[2*i], points, 16);
        if(s < 0) ERR("read");
        if(s == 0) exit(EXIT_SUCCESS);
        int num = atoi(points);
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

//ignorowanie SIGPIPE zeby obslugiwac bledy 
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) ERR("signal");

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

    int players_points[n];
    for(int i = 0; i < n; i++) players_points[i] = 0;

    int active[n]; //tablica aktywnych graczy
    for(int i = 0; i < n; i++) active[i] = 1;

    for(int r = 0; r < m; r++){
        printf("NEW ROUND\n");
        for(int i = 0; i < n; i++){ //wysylamy new_round tylko do aktywnych
            if(!active[i]) continue;
            char new_round[16]; 
            memset(new_round, 0, 16);
            snprintf(new_round, 16, "new_round");
            if(write(to_player[2*i + 1], new_round, 16) < 0){
                printf("player %d failed\n", i);
                active[i] = 0;
                close(to_player[2*i + 1]);
                close(from_player[2*i]);
            }
        }

        int players_cards[n];
        int played[n]; //gracze ktorzy na prawde wyslali karte w tej rundzie

        for(int i = 0; i < n; i++){
            players_cards[i] = -1;
            played[i] = 0;
        }

        for(int i = 0; i < n; i++){
            if(!active[i]) continue; //odbieranie kart tylko od aktywnych
            char message[16];
            ssize_t s = read(from_player[2*i], message, 16);
            if(s <= 0){
                printf("player %d failed\n", i);
                active[i] = 0;
                close(to_player[2*i + 1]);
                close(from_player[2*i]);
            }else{
                players_cards[i] = atoi(message);
                played[i] = 1;
            }
        }

        int played_count = 0; //ile graczy zagralo w tej rundzie
        for(int i = 0; i < n; i++){
            if(played[i]) played_count++;
        }
        if(played_count == 0){ //jesli nikt nie zagral to konczymy
            printf("all players failed\n");
            break;
        }

//kto zagral:
        for(int i = 0; i < n; i++){
            if(played[i]){
                printf("player %d played %d\n", i, players_cards[i]);
            }
        }

//maksimum sposrod grajacych
        int first = -1;
        for(int i = 0; i < n; i++){
            if(played[i]) {
                first = i;
                break;
            }
        }
        int max_val = players_cards[first];
        for(int i = 0; i < n; i++){
            if(played[i] && players_cards[i] > max_val) max_val = players_cards[i];
        }
//ilu jest zwyciezcow sposrod grajacych
        int winners_count = 0;
        for(int i = 0; i < n; i++){
            if(played[i] && players_cards[i] == max_val) winners_count++;
        }
//ile punktow przypada na zwyciezce grajacych jest played_count!
        int round_points = played_count / winners_count;
//dopisujemy punkty do tabeli wynikow do graczy ktorzy naprawde zagrali!
        for(int i = 0; i < n; i++){
            if(played[i] && players_cards[i] == max_val) players_points[i] += round_points;
        }
//zwyciezcy rundy
        printf("round %d winners:", r + 1);
        for(int i = 0; i < n; i++){
            if(played[i] && players_cards[i] == max_val){
                printf(" %d", i);
            }
        }
        printf("\n");
//odsylamy grajacym punkty za te runde 
        for(int i = 0; i < n; i++){
            if(!active[i]) continue;
            char points[16];
            memset(points, 0, 16);
            if(played[i] && players_cards[i] == max_val) { //zwyciezcy
                snprintf(points, 16, "%d", round_points);
            }
            else { //przegrani
                snprintf(points, 16, "%d", 0);
            }
                //jesli nie da sie wyslac do gracza uznajemy go za martwego
            if(write(to_player[2*i + 1], points, 16) < 0){
                printf("player %d failed\n", i);
                active[i] = 0;
                close(to_player[2*i + 1]);
                close(from_player[2*i]);
            }
        }
    }

    for(int i = 0; i < n; i++){
        printf("player %d got %d points\n", i, players_points[i]);
    }
    
//zamykamy deskryptory aktywnym gracza
    for(int i = 0; i < n; i++){
        if(active[i]){
            if(close(to_player[2*i + 1]) < 0) ERR("close");
            if(close(from_player[2*i]) < 0) ERR("close");
        }
    }

    while(wait(NULL) > 0);
    if(errno != ECHILD) ERR("wait");

//koncowy zwyciezca
    int best = players_points[0];
    for(int i = 1; i < n; i++){
        if(players_points[i] > best) best = players_points[i];
    }

    printf("GAME WINNER(S):");
    for(int i = 0; i < n; i++){
        if(players_points[i] == best) printf(" %d", i);
    }
    printf("\n");

    free(to_player);
    free(from_player);
    free(pids);
    return EXIT_SUCCESS;
}
