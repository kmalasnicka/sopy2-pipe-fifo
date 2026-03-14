#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <sys/types.h>
#include <ctype.h> 
#include <limits.h> //PIPE_BUF

//kient -> fifo -> serwer
//klient: otwiera plik, czyta dane z pliku, wysyla je do fifo, konczy gdzy wysle caly plik
//serwer: tworzy fifo, czeka na dane od klientow w kawalkach o rozmiarze PIPE_BUFF, wypisuje dane razem z PID, usuwa FIFO z systemu plikow

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char **argv){
    fprintf(stderr, "usage %s: fifo_file\n", argv[0]);
    exit(EXIT_FAILURE);
}

void read_from_fifo(int fifo){
    ssize_t count, i; 
    char buffer[PIPE_BUF]; 
    do{
        if((count = read(fifo, buffer, PIPE_BUF)) < 0) ERR("read");  //serwer probuje przeczytac PIPE_BUF bajtow z FIFO
        if(count > 0){ //jesli cos przeczytano
            printf("\nPID: %d\n", *((pid_t *)buffer)); //wyswietla pid, (pid_t *)buffer - poczatek bufora to pid, *((pid_t*)buffer) - wartosc poczatku bufora 
            for(i = sizeof(pid_t); i < PIPE_BUF; i++){ //od miejsca za pid wypisujemy bajty
                if(isalnum(buffer[i])) printf("%c", buffer[i]);
            }
        } 
    } while (count > 0); //serwer czyta dopoki klienci pisza do fifo czy zamkna fifo read() = 0 i petla sie konczy
}

int main(int argc, char **argv){
    int fifo; 
    if(argc != 2) usage(argc, argv);
    if(mkfifo(argv[1], S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) < 0){ 
        if(errno != EEXIST) ERR("create fifo"); 
    }
    if((fifo = open(argv[1], O_RDONLY)) < 0) ERR("open"); 
    read_from_fifo(fifo);
    if(close(fifo) < 0) ERR("close fifo"); 
    if(unlink(argv[1]) < 0) ERR("remove fifo");
    return EXIT_SUCCESS;
}
