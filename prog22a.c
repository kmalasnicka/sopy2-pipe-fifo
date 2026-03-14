#define _GNU_SOURCE
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

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s n, 0<n<=10", argv[0]);
    exit(EXIT_FAILURE);
}

int sethandler(void (*f)(int), int sigNo){//obsluga sygnalu
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL)) return -1;
    return 0;
}

void sigchld_handler(int sig){ //funkcja ktora jest uruchamiana za kazdym razem jak dziecko sie zakonczy
    pid_t pid;
    for (;;){
        pid = waitpid(0, NULL, WNOHANG);
        if (0 == pid)
            return;
        if (0 >= pid){
            if (ECHILD == errno) return;
            ERR("waitpid:");
        }
    }
}

//musimy zamykac deskryptory bo read() zwroci 0 tylko wtedy kiedy zaden proces nie ma otwratego konca zapisu tej rury
//dlatego: dzieci zamykaja R[0], parent zamyka R[1]

void parent_work(int R_read){
    char c;
    int count;
    //jesli read jest przerywany przez sygnaly to mozemy uzyc count = TEMP_FAILURE_RETRY(read(R_read, &c, 1));
    while((count = TEMP_FAILURE_RETRY(read(R_read, &c, 1))) == 1){ //rodzic czyta z rury dopoki przychodza dane, konczy gdzy read() = 0 czyli eof
        printf("%c", c);
    }
    if(count < 0) ERR("read");
    printf("\n");
}

void child_work(int R_write){
    srand(getpid()); //zeby dla kazdego dziecka bylo unique 
    char random_char = 'a' + rand() % ('z' - 'a' + 1);
    if(write(R_write, &random_char, 1) < 0) ERR("write");
    exit(EXIT_SUCCESS);
}

void create_children(int n, int *children_pipes, int *R){
    for(int i = 0; i < n; i++){
        pid_t pid = fork(); //po fork dziecko dostaje kopie wszystkich pipeow
        if(pid < 0) ERR("fork");
        if(pid == 0){
            if(close(R[0]) < 0) ERR("close"); //dziecko nie czyta z R
            for(int i = 0; i < n * 2; i++){ //prywatne pipey tez nie sa uzywane wiec zamykamy wszystkie 
                if(close(children_pipes[i]) < 0) ERR("close");
            }
            free(children_pipes); //po fork dziecko dostaje kopie pamieci procesu wiec zwolnic pamiec
            child_work(R[1]);
        }
    }
}

int main(int argc, char **argv){
    if(argc != 2) usage(argc, argv); 

    int n = atoi(argv[1]);
    if(n <= 0 || n > 10) usage(argc, argv);

    int R[2]; //R[0] - koniec do czytania, R[1] - koniec do pisania
    if(pipe(R) < 0) ERR("pipe");

    if(sethandler(sigchld_handler, SIGCHLD)) ERR("setting parent SIGCHLD"); //SIGCHLD - dziecko konczy dzialanie, sygnal do rodzica

    int *children_pipes = malloc(2 * n * sizeof(int)); //kazde dziecko ma swoj pipe: children_pipes[2*i] - read end, children_pipes[2*i+1] - write end 
    if(!children_pipes) ERR("malloc");

    for(int i = 0; i < n; i++){
        if(pipe(&children_pipes[i * 2]) < 0) ERR("pipe");
    }

   create_children(n, children_pipes, R);

    if(close(R[1]) < 0) ERR("close"); //parent nie pisze nic do R wiec zamykamy
    for(int i = 0; i < n * 2; i++){ //parent nie uzywamy prywatnych pipeow dzieci wiec je zamykamy
        if(close(children_pipes[i]) < 0) ERR("close");
    }
    parent_work(R[0]);
    if(close(R[0]) < 0) ERR("close");

    //alternatywa do obslugiwania sygnalow program czeka na koncu az dzieci skoncza
    //while(wait(NULL) > 0);
    free(children_pipes);
    return EXIT_SUCCESS;
}

