#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_MSG 16

void write_message(int fd, char *msg){
    int len = strlen(msg) + 1; //liczymy dlugosc napisu np strlen("42") = 2 i dodajemy 1 bo na koncu jest '\0'
    if(write(fd, msg, len) < 0) ERR("write"); //wysylamy caly napis przez pipe
}

void read_message(int fd, char *buf, int size){
    int i = 0;
    char c; //czytamy chwilowo odczytany znak
    while(1){
        if(read(fd, &c, 1) < 0) ERR("read"); //czytamy 1 bajt z pipe
        buf[i++] = c; //wkladamy ten znak do bufora
        if(c == '\0') break; //jesli koniec napisu to konczymy
        if(i >= size){ //sprawdzamy czy bufor nie jest za duzy
            fprintf(stderr, "message too long\n");
            exit(EXIT_FAILURE);
        }
    }
}

void create_children(int n, int *R1, int *R2, int *R3){
    for(int i = 2; i < 2 + n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            char send_buf[MAX_MSG]; //wiadomosc do wyslania
            char recv_buf[MAX_MSG]; //odebrana wiadomosc
            int rand_num, recv_num;
            srand(getpid());
            if(i == 2){
                if(close(R1[1]) < 0) ERR("close");
                if(close(R2[0]) < 0) ERR("close");
                if(close(R3[0]) < 0) ERR("close");
                if(close(R3[1]) < 0) ERR("close");

                rand_num = rand() % 100;
                //snprintf zamienia liczbe na napisz
                snprintf(send_buf, MAX_MSG, "%d", rand_num); //przyjmuje bufor, jego rozmiar i ta liczbe
                write_message(R2[1], send_buf);
                read_message(R1[0], recv_buf, MAX_MSG);
                recv_num = atoi(recv_buf); //recv_buf odbiera wiadomosc jako tekst i zamieniamy to na liczbe 
                printf("second process pid: %d, recieved number: %d\n", getpid(), recv_num);
                
                if(close(R1[0]) < 0) ERR("close");
                if(close(R2[1]) < 0) ERR("close");
            } else if (i == 3){
                if(close(R1[0]) < 0) ERR("close");
                if(close(R1[1]) < 0) ERR("close");
                if(close(R2[1]) < 0) ERR("close");
                if(close(R3[0]) < 0) ERR("close");

                rand_num = rand() % 100;
                snprintf(send_buf, MAX_MSG, "%d", rand_num);
                write_message(R3[1], send_buf);
                read_message(R2[0], recv_buf, MAX_MSG);
                recv_num = atoi(recv_buf);
                printf("third process pid: %d, recieved number: %d\n", getpid(), recv_num);

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

    create_children(2, R1, R2, R3);

    char send_buf[MAX_MSG];
    char recv_buf[MAX_MSG];
    int rand_num, recv_num;   
    srand(getpid());

    if(close(R1[0]) < 0) ERR("close");
    if(close(R2[0]) < 0) ERR("close");
    if(close(R2[1]) < 0) ERR("close");
    if(close(R3[1]) < 0) ERR("close");

    rand_num = rand() % 100;
    snprintf(send_buf, MAX_MSG, "%d", rand_num);
    write_message(R1[1], send_buf);
    read_message(R3[0], recv_buf, MAX_MSG);
    recv_num = atoi(recv_buf);
    printf("parent process pid: %d, recieved numer: %d\n", getpid(), recv_num);

    if(close(R1[1]) < 0) ERR("close");
    if(close(R3[0]) < 0) ERR("close");

    while(wait(NULL) > 0);
    if(errno != ECHILD) ERR("wait"); //gdy nie ma dzieci wait() ustawia errno = ECHILD 
    return EXIT_SUCCESS;
}