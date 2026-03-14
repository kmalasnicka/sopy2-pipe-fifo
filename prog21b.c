#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <sys/types.h>
#include <ctype.h> 
#include <string.h>
#include <limits.h> //PIPE_BUF

//kient -> fifo -> serwer
//klient: otwiera plik, otwiera fifo do pisania, czyta plik kawalkami, kazdy kawalek wysyla do fifo + pid procesu klienta - taki blok ma miec PIPE_BUF
//serwer: tworzy fifo, czeka az klienci beda cos wysylac, czyta dane z fifo, usuwa znaki niealfabetyczne, wypisuje wynik na ekran

//zapis do pipe/fifo o rozmiarze nie wiekszym nic PIPE_BUF jest atomowy, nie pomieszaja sie dane
#define MSG_SIZE (PIPE_BUF - sizeof(pid_t)) //caly blok wyslany do fifo -> pid + pipe_buf
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char **argv){
    printf("usage %s: fifo_file file\n", argv[0]);
    exit(EXIT_FAILURE);
}

//czyta dane z pliku, pakuje do blokow, dopisuje pid i wysyla do fifo
void write_to_fifo(int fifo, int file){ 
    int64_t count; //liczba przeczytamych bajtow z pliku
    char buffer[PIPE_BUF]; //blok ktory bedzie wysylany przez fifo
    char *buf; //wskaznik na miejsce w buffer gdzie zaczynaja sie dane pliku
    *((pid_t *)buffer) = getpid(); //wpisujemy pid na poczatek bufora
    buf = buffer + sizeof(pid_t); //ustawiamy wskaznik buf zaraz za pid
    do{
        if((count = read(file, buf, MSG_SIZE)) < 0) ERR("read"); //czytamy z pliku od miejsca wskazywanego przez buf maksymalnie MSG_SIZE bajtow
        if(count < MSG_SIZE) memset(buf + count, 0, MSG_SIZE - count); //jesli ostatni fragment jest krotszy niz MSG_SIZE to uzupelniamy zerami
        if(count > 0) { //jesli count = 0 to koniec pliku wiec nie robmy write
            if(write(fifo, buffer, PIPE_BUF) < 0) ERR("write"); //wysylanie do fifo
        } 
    } while (count == MSG_SIZE); //az uda sie przeczytac pelen kawalek danych
}

int main(int argc, char **argv){
    int fifo, file; // deskryptory do fifo, plik wejsciowy
    if(argc != 3) usage(argc, argv);
    if(mkfifo(argv[1], S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) < 0){ 
        if(errno != EEXIST) ERR("create fifo"); 
    }
    if((fifo = open(argv[1], O_WRONLY)) < 0) ERR("open"); //otwieramy fifo do pisania
    if((file = open(argv[2], O_RDONLY)) < 0) ERR("open"); //otwieramy plik do czytania danych z 
    write_to_fifo(fifo, file); //wysylanie danych
    if(close(file) < 0) ERR("close file");
    if(close(fifo) < 0) ERR("close fifo");  
    return EXIT_SUCCESS;
}
