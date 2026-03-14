#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h> //open() i O_RDONLY
#include <sys/stat.h> //mkfifo()
#include <unistd.h> //read(), close()
#include <sys/types.h>
#include <ctype.h> //isalnum()

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

//uproszczona wersja komenda cat robi jako klient
//serwer tworzt fifo, czyta z niego, zostawia tylko litery i cyfry i wypisuje na ekran
void usage(int argc, char **argv){
    printf("usage %s: fifo file\n", argv[0]);
    exit(EXIT_FAILURE);
}

void read_from_fifo(int fifo){
    ssize_t count; //ile bajtow przeczytal read
    char c; //jeden przeczytany znak
    do{
        if((count = read(fifo, &c, 1)) < 0) ERR("read"); //czytaj z fifo, zapisz do c, maksymalnie 1 bajt, czyli czytamy znak po znaku
        if(count > 0 && isalnum(c)) printf("%c", c); //jesli to jest znak alfanumeryczny to wypisujemy
    } while (count > 0); //czytamy tak dlugo az read() == 0 czyli eof koniec danych np writer zamknal fifo
}

int main(int argc, char **argv){
    int fifo; //deskryptor fifo, liczba zwrocona przez open()
    if(argc != 2) usage(argc, argv);
    if(mkfifo(argv[1], S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) < 0){ //tworzymy fifo o podanej w argumencje nazwie, wlasciciel i grupa moze czytac i pisac
        if(errno != EEXIST) ERR("create fifo"); //EEXIST czyli fifo juz istnieje, jesli to nie to errno to wystapil blad przy tworzeniu
    }
    if((fifo = open(argv[1], O_RDONLY)) < 0) ERR("open"); //otwiera fifo do odczytu
    read_from_fifo(fifo);
    if(close(fifo) < 0) ERR("close fifo"); //zamykamy deskryptor po skonczeniu czytania
    return EXIT_SUCCESS;
}

//cat plik > myfifo - owtiera plik czyta jego zawartosc i zapisuje dane do FIFO fmyifo