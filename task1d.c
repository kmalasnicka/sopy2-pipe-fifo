#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_MSG 16

volatile sig_atomic_t work = 1; //globalna zmienna ktora mowi czy proces ma jeszcze pracowac

void sig_handler(int sig){ //gdy przyjdzie SIGINT to proces ustawia work = 0, dzieki czemu while(work) sie skoncza
    work = 0;
}

int sethandler(void (*f)(int), int sigNo){ //ustawia handler dla sygnału
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if(sigaction(sigNo, &act, NULL) == -1) return -1;
    return 0;
}

int write_message(int fd, char *msg){
    int len = strlen(msg) + 1; //liczymy dlugosc napisu np strlen("42") = 2 i dodajemy 1 bo na koncu jest '\0'
    int written = 0; //licnik zapisanych bajtow
    while(written < len){ //dopoki nie wyslemy calej wiadomosci
        int tmp = write(fd, msg + written, len - written); //zapisujemy do deskryptora, jesli zapisze tylko czesc to reszte zapisuje w kolejnych iteracjach
        if(tmp < 0){ //jesli write zwroci blad 
            if(errno == EINTR) { ///sygnal przerwal write
                if(!work) return 0; //jesli to byl sigint przerywamy
                continue; //inaczej kontynuujemy
            }
            if(errno == EPIPE) return 0; //jesli pipe zostal zerwany zwracamy 0
            ERR("write");
        }
        written += tmp;
    }
    return 1;
}

int read_message(int fd, char *buf, int size){
    int i = 0;
    char c; //czytamy chwilowo odczytany znak
    while(1){
        int r = read(fd, &c, 1); //czytamy 1 bajt
        if(r < 0){
            if(errno == EINTR){
                if(!work) return 0;
                continue;
            }
            ERR("read");
        }
        if(r == 0) return 0; //eof - pipe zamkniety
        buf[i++] = c; //dopisujemy bajt do bufora
        if(c == '\0') break; //konczymy jak przeczytamy '\0'
        if(i >= size){
            fprintf(stderr, "message too long\n");
            exit(EXIT_FAILURE);
        }
    }
    return 1;
}

void create_children(int n, int *R1, int *R2, int *R3){
    for(int i = 2; i < 2 + n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            char buf[MAX_MSG];
            char new_buf[MAX_MSG];
            int rand_number;
            srand(getpid());
            if(i == 2){
                if(close(R1[1]) < 0) ERR("close");
                if(close(R2[0]) < 0) ERR("close");
                if(close(R3[0]) < 0) ERR("close");
                if(close(R3[1]) < 0) ERR("close");

                while(work){
                    if(!read_message(R1[0], buf, MAX_MSG)) break;
                    printf("second process pid: %d, received number: %s\n", getpid(), buf);
                    int num = atoi(buf);
                    if(num == 0) break;
                    rand_number = (rand() % 21) - 10; //losowa liczba z przedzialu [-10, 10]
                    int send_num = num + rand_number; //wyliczamy nowa wartosc
                    snprintf(new_buf, MAX_MSG, "%d", send_num); //zmieniamy liczbe na napis
                    if(!write_message(R2[1], new_buf)) break; //wysylamy nowa wiadomosc
                }

                if(close(R1[0]) < 0) ERR("close");
                if(close(R2[1]) < 0) ERR("close");
            } else if (i == 3){
                //czytamy i wysylamy to co przeczytalismy 
                if(close(R1[0]) < 0) ERR("close");
                if(close(R1[1]) < 0) ERR("close");
                if(close(R2[1]) < 0) ERR("close");
                if(close(R3[0]) < 0) ERR("close");

                while(work){
                    if(!read_message(R2[0], buf, MAX_MSG)) break;
                    printf("third process pid: %d, received number: %s\n", getpid(), buf);
                    int num = atoi(buf);
                    if(num == 0) break;
                    rand_number = (rand() % 21) - 10;
                    int send_num = num + rand_number;
                    snprintf(new_buf, MAX_MSG, "%d", send_num);
                    if(!write_message(R3[1], new_buf)) break;
                }

                if(close(R2[0]) < 0) ERR("close");
                if(close(R3[1]) < 0) ERR("close");
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

    //ustawiamy handler wygnalu
    if(sethandler(sig_handler, SIGINT)) ERR("setting SIGINT handler");
    if(sethandler(SIG_IGN, SIGPIPE)) ERR("setting SIGPIPE handler"); //ignorujemy SIGPIPE zeby zamiast zabicia procesu dostac EPIPE w write
    create_children(2, R1, R2, R3);

    char buf[MAX_MSG];
    char new_buf[MAX_MSG];
    srand(getpid());

    if(close(R1[0]) < 0) ERR("close");
    if(close(R2[0]) < 0) ERR("close");
    if(close(R2[1]) < 0) ERR("close");
    if(close(R3[1]) < 0) ERR("close");

    strcpy(buf, "1"); //kopjujemy 1 do buf
    write_message(R1[1], buf); //parent wysyla 1 w obieg

    while(work){
        if(!read_message(R3[0], buf, MAX_MSG)) break;
        printf("parent process pid: %d, received number: %s\n", getpid(), buf);
        int num = atoi(buf);
        if(num == 0) break;
        int rand_num = (rand() % 21) - 10;
        int send_num = num + rand_num;
        snprintf(new_buf, MAX_MSG, "%d", send_num);
        if(!write_message(R1[1], new_buf)) break;
    }

    if(close(R1[1]) < 0) ERR("close");
    if(close(R3[0]) < 0) ERR("close");

    while(wait(NULL) > 0);
    if(errno != ECHILD) ERR("wait"); //gdy nie ma dzieci wait() ustawia errno = ECHILD 
    return EXIT_SUCCESS;
}