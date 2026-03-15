#define _GNU_SOURCE //rozszerzenie potrzebne do TEMP_FAILURE_RETRY
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_BUF 200 //maksymalna dlugosc danych wysylanych przez dziecko
volatile sig_atomic_t last_signal = 0; //zmienna do ostatniego sygnalu

//1 wspolny pipe R - wszystkie dzieci pisza do rodzica
//n prywatnych pipeow - rodzic pisze osobno do kazdego dziecka
//rodzic: ignoruje SIGINT na poczatku tworzenia dzieci, ignoruje SIGPIPE, ma SIGCHLD handler, po ctr+c wybiera losowe dziecko, losuje litere a-z wysyla ja do pipe tego dziecka
//dziecko: ustawia handler SIGINT na sig_killme, jak dostanie SIGINT to z 20% szans konczy dzialanie, po otrzymaniu znaku tworzy bufor o randomowej dlugosci miedzy 1-200, buf[0] przechpwije dligosc komunikatu reszta to ten znak, wysyla komunikat do rodzica przez R
//rodzic: wypisuje wszysto co dostanie z R, konczy sie gdy wszytskie dzieci znikna 

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s n, 0<n<=10", argv[0]);
    exit(EXIT_FAILURE);
}

int sethandler(void (*f)(int), int sigNo){ //ustawia handler sygnalu
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL)) return -1;
    return 0;
}

void sig_handler(int sig) {last_signal = sig;} //handler zapamietujacy sygnal
 
void sig_killme(int sig){ //handler dziecka
    if(rand() % 5 == 0) exit(EXIT_SUCCESS); //dziecko po sigint ma 20% szans umrzec, losuje liczbe 0..4
}

void sigchld_handler(int sig){ //funkcja sprzetajaca zakonczone dzieci zeby nie zostawaly zombie
    pid_t pid;
    for (;;){
        pid = waitpid(0, NULL, WNOHANG);
        if (0 == pid)
            return;
        if (0 >= pid){
            if (ECHILD == errno) return; //nie ma juz zadnych dzieci
            ERR("waitpid:");
        }
    }
}

void parent_work(int n, int *children_pipes, int R_read){ //R_read - deskryptor odczytu wspolnego pipe
    //unsigned bo signed jest od -128 do 127 i jak dziecko wsysle rozmiar 200 to rodzic moze odczytac jako l ujemna
    unsigned char size; //rozmiar wiadomosci od dziecka, unsigned char ma 1 bajt - moze przechowywac wartosc 0..255
    char c; //znak wyslany do dziecka
    char buf[MAX_BUF + 1]; //bufor na dane do wypisania
    int status; 
    
    srand(getpid());
    if(sethandler(sig_handler, SIGINT)) ERR("setting SIGINT handler in parent"); //po ctr+c sygnal zapisywany jest do last_signal i rodzic nie umrze od razu
    
    while(1){
        if(last_signal == SIGINT){ //jesli byl sygnal ctr+c
            int i = rand() % n; //losujemy indeks dziecka
            //szuka pierwsze aktywne dziecko, ktorego pipe do zapisu nie jest zamkniety
            int found = 0; //flaga mowiaca czy znaleziono dziecko
            for(int k = 0; k < n; k++){ //przesuwamy sie po dzieciach
                int idx = (i + k) % n; //indeks aktualnie sprawdzane dziecko, %n sprawia ze po ostatnim dziecku wracamy do pierwszego
                if(children_pipes[2*idx + 1] != 0){ //sprawdzamy czy pipe zapisu dla dziecka idx jest aktywny
                    i = idx; //jesli znalezlismy aktywne dziecko ustawiamy idx jako i 
                    found = 1;
                    break; //konczymy petle 
                }
            }
            if(found && children_pipes[2*i + 1] != 0){ //zanleziono aktywne dziecko
                c = 'a' + rand() % ('z' - 'a' + 1); //losujemy litere
                status = TEMP_FAILURE_RETRY(write(children_pipes[2 * i + 1], &c, 1)); //wysylamy litere do dziecka
                if(status != 1){ //sprawdzamy czy zapis sie udal
                    if(TEMP_FAILURE_RETRY(close(children_pipes[2 * i + 1])) < 0) ERR("close"); //jelsi zapis sie nie udal to zamykamy ten deskryptor
                    children_pipes[2 * i + 1] = 0; //oznaczamy pipe jako nieaktywny
                }
            }
            last_signal = 0; //po obsluzeniu sygnalu zerujemy zmienna globalna, sygnal zostal obsluzony
        }
        status = read(R_read, &size, 1); //odczytujemy rozmiar wiadomosci, odczytujemy 1 bajt ktory zawiera dlugosc wiadomosci, dziecko wysyla np: 5aaaaa
        if(status < 0 && errno == EINTR) continue; //jesli read zostal przerwany przez sygnal, wracamy do petli
        if(status < 0) ERR("read from R"); //blad odczytu inny niz EINTR
        if(status == 0) break; //koniec pipea, gdy read() = 0 to eof czyli wszyscy piszacy na R zamkneli swoje konce lub dzieci zniknely
        if(TEMP_FAILURE_RETRY(read(R_read, buf, size)) < size) ERR("read from R"); //po odczytaniu size rodzic czyta dokladnie size bajtow danych do bufora buf
        buf[(int)size] = '\0'; //null terminator na koncu
        printf("%s\n", buf); //wypisujemy wiadomosc 
    }
}

void child_work(int fd, int R_write){//fd - prywatny pipe dziecka do czytania od rodzica, R_write - koniec zapisu wspolnego pipe R
    char c; //znak odebrany od rodzica
    char buf[MAX_BUF + 1]; //bufor do wyslania do rodzica
    unsigned char size; //ile znakow dziecko wygeneruje
    srand(getpid()); 
    if(sethandler(sig_killme, SIGINT)) ERR("setting SIGINT handler for child"); //handler SIGINT ktory z 20% szans je zakonczy
    while(1){
        if(TEMP_FAILURE_RETRY(read(fd, &c, 1)) < 1) ERR("read from child pipe"); //czekamy az rodzic wysle jeden znak do prywatnego pipe
        size = 1 + rand() % MAX_BUF; //losujemy dlugosc wiadomosci od 1 do 200
        buf[0] = size; //zapisujemy dligosc w pierwszym bajcie bufora
        memset(buf + 1, c, size); //wypelniamy size bajtow znakiem c zaczynajac od buf[1]
        if(TEMP_FAILURE_RETRY(write(R_write, buf, size + 1)) < 0) ERR("write to R"); //wysylamy do rodzica przez wspolny pipe R buf
    }
}

void create_children(int n, int *children_pipes, int *R){
    for(int i = 0; i < n; i++){ //tworzymy n dzieci
        pid_t pid = fork(); //po fork dziecko dostaje kopie wszystkich pipeow
        if(pid < 0) ERR("fork");
        if(pid == 0){ //dziecko
            if(TEMP_FAILURE_RETRY(close(R[0])) < 0) ERR("close"); //dziecko nie czyta z R
            for(int j = 0; j < n; j++){ //przechodzimy po wszystkich dzieciach
                if(j != i){ //zamkniecie koncow odczytow innych dzieci, bo dziecko i ma czytac tylko ze swojego pipe
                    if(TEMP_FAILURE_RETRY(close(children_pipes[2 * j])) < 0) ERR("close");
                }
                if(TEMP_FAILURE_RETRY(close(children_pipes[2 * j + 1])) < 0) ERR("close"); //zamyka wszystkie write end bo dzieci nigdy nie pisza do tych pipeow
            }
            child_work(children_pipes[2 * i], R[1]);
            free(children_pipes); //po fork dziecko dostaje kopie pamieci procesu wiec zwolnic pamiec
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char **argv){
    if(argc != 2) usage(argc, argv); 

    int n = atoi(argv[1]);
    if(n <= 0 || n > 10) usage(argc, argv);

    int R[2]; //R[0] - koniec do czytania, R[1] - koniec do pisania
    if(pipe(R) < 0) ERR("pipe"); //tworzy pipe R

    if(sethandler(SIG_IGN, SIGINT)) ERR("setting SIGINT handler"); //na poczatku rodzic ignoruje SIGINT, oblsuguje po stworzeniu dzieci
    if(sethandler(SIG_IGN, SIGPIPE)) ERR("setting SIGPIPE handler"); //ignoruje SIGPIPE, po zignorowaniu jest tylko blad write, robimy bo jak write sie nie uda to rodzic ma zauwazyc blad i zamknac deskryptor dziecka
    if(sethandler(sigchld_handler, SIGCHLD)) ERR("setting parent SIGCHLD"); //rodzic ustawia handler SIGCHLD zeby sprzatac zakonczone dzieci

    int *children_pipes = malloc(2 * n * sizeof(int));  //pamiec na n pipeow kazy ma 2 konce
    if(!children_pipes) ERR("malloc");

    for(int i = 0; i < n; i++){ //tworzenie pipeow
        if(pipe(&children_pipes[i * 2]) < 0) ERR("pipe");
    }

   create_children(n, children_pipes, R); //tworzenie dzieci

    if(TEMP_FAILURE_RETRY(close(R[1])) < 0) ERR("close"); //rodzic zamyka koniec zapisu wspolnego pipe R bo sam z niego tylko czyta
    for(int i = 0; i < n; i++){ 
        if(TEMP_FAILURE_RETRY(close(children_pipes[2*i])) < 0) ERR("close"); //zamykamy konce odczytu prywatnych pipeow dzieci
    }
    parent_work(n, children_pipes, R[0]);
//po zakonczeniu rodzic zamyka wszystkie jeszcze otwarte koncze zapisu prywatnych pipeow
    for(int i = 0; i < n; i++){
        if(children_pipes[2 * i + 1]){ //jesli jest children_pipes[2 * i + 1] = 1 to jest aktywny
            if(TEMP_FAILURE_RETRY(close(children_pipes[2 * i + 1])) < 0) ERR("close");
        }
    }
    if(TEMP_FAILURE_RETRY(close(R[0])) < 0) ERR("close"); //zamykamy koniec odczytu R
    free(children_pipes);
    return EXIT_SUCCESS;
}

