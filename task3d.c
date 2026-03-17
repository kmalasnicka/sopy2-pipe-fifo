#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
sig_atomic_t volatile stop = 0;

void sig_handler(int sig){
    stop = 1;
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

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s 3<=n<=20", argv[0]);
    exit(EXIT_FAILURE);
}

void msleep(int ms)
{
    struct timespec tt;
    tt.tv_sec = ms / 1000;
    tt.tv_nsec = (ms % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}


void student_work(int i, int n, int *students_pipe, int *teacher_pipes){
    char message[64];
    char response[64];

    printf("Student: %d\n", getpid());

    if(close(students_pipe[0]) < 0) ERR("close"); //nie czyta ze wspolnego pipe
    for(int j = 0; j < n; j++){ //j zeby sie nie mieszalo z i!!!
        //zamykamy konce pipe teachera oprocz swojego read end
        if(j == i){
            if(close(teacher_pipes[2*j + 1]) < 0) ERR("close"); //zamykamy write end swojego pipe
        } else {
            if(close(teacher_pipes[2*j]) < 0) ERR("close"); //cudzy read end
            if(close(teacher_pipes[2*j + 1]) < 0) ERR("close"); //cudzy write end
        }
    }
//czekamy na pytanie od nauczyciela
    if(read(teacher_pipes[2*i], message, 64) < 0) ERR("read"); // opcjonalnie mozna wypisac printf("%s\n", message)
    snprintf(response, 64, "Student %d: HERE!", getpid());
    if(write(students_pipe[1], response, 64) < 0) ERR("write");

    int passed_or_failed;
    srand(getpid() ^ time(NULL));
    int k = (rand() % 7) + 3; // skill level
    int stage = 0;
    while(stage < 4){
        int t = (rand() % 401) + 100;
        msleep(t);

        int q = (rand() % 20) + 1;
        int attempt_score = k + q;

        char id_attempt_score[64];
        snprintf(id_attempt_score, 64, "%d %d", getpid(), attempt_score);
        
        if(write(students_pipe[1], id_attempt_score, 64) < 0) {
            if(errno == EPIPE) {
                printf("Student %d: Oh no, I haven't finished stage %d. I need more time.\n", getpid(), stage + 1);
                break;
            }
            ERR("write");
        }
        int r = read(teacher_pipes[2*i], &passed_or_failed, sizeof(int));
        if(r < 0) ERR("read");
        if(r == 0){
            printf("Student %d: Oh no, I haven't finished stage %d. I need more time.\n", getpid(), stage + 1);
            break;
        } 
        
        if(passed_or_failed == 1){ //jesli jest 0 to student probuje dalej
             stage++;
        }
    } 
    if(stage == 4) printf("Student %d: I NAILED IT!\n", getpid());
    if(close(teacher_pipes[2*i]) < 0) ERR("close");
    if(close(students_pipe[1]) < 0) ERR("close");
}

void create_students(int n, int *students_pipe, int *teacher_pipes, pid_t *pids){
    for(int i = 0; i < n; i++){
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        if(pid == 0){
            student_work(i, n, students_pipe, teacher_pipes);
            exit(EXIT_SUCCESS);
        }
        pids[i] = pid;
    }
}

int main(int argc, char **argv){
    if(argc != 2) usage(argc, argv);

    int n = atoi(argv[1]);
    if(n < 3 || n > 20) usage(argc, argv);

    if(sethandler(sig_handler, SIGALRM)) ERR("setting SIGALRM handler");
    if(sethandler(SIG_IGN, SIGPIPE)) ERR("setting SIGPIPE handler");

    int students_pipe[2];
    int *teacher_pipes = malloc(sizeof(int) * n * 2);
    pid_t *pids = malloc(sizeof(pid_t) * n);
    if(!teacher_pipes || !pids) ERR("malloc");

    if(pipe(students_pipe) < 0) ERR("pipe");
    for(int i = 0; i <n; i++){
        if(pipe(&teacher_pipes[2*i]) < 0) ERR("pipe");
    }
    printf("Teacher: %d\n", getpid());
    create_students(n, students_pipe, teacher_pipes, pids);

    if(close(students_pipe[1]) < 0) ERR("close");
    for(int i = 0; i < n; i++){
        if(close(teacher_pipes[2*i]) < 0) ERR("close");
    }

    char message[64];
    char response[64];
    for(int i = 0; i < n; i++){
        snprintf(message, 64, "Teacher: Is %d here?", pids[i]);
        printf("%s\n", message);
        if(write(teacher_pipes[2*i + 1], message, 64) < 0) ERR("write");
    }
    for(int i = 0; i < n; i++){
        if(read(students_pipe[0], response, 64) < 0) ERR("read");
        printf("%s\n", response);
    }

    alarm(2); //za 10 sekund proces dostanie SIGALRM
    
    int current_stage[n]; //tablica aktualnych etapow studentow
    int stage_points[] = {3, 6, 7, 5};
    int finished_students = 0;

    for(int i = 0; i < n; i++){
        current_stage[i] = 1;
    }

    srand(getpid());

    while(finished_students < n && !stop){
        char id_attempt_score[64];
        int r = read(students_pipe[0], id_attempt_score, 64);
        if(r < 0){
            if(errno == EINTR && stop) break;
            ERR("read");
        }
        if(r == 0) break;
        
        pid_t pid;
        int attempt_score;
        int passed = 1;
        int failed = 0;
        
        if(sscanf(id_attempt_score, "%d %d", &pid, &attempt_score) != 2) ERR("sscanf");
        
        int j = -1;
        for(int i = 0; i < n; i++){
            if(pid == pids[i]) {
                j = i; //szukamy indeksu tego studenta zeby mu wyslac wiadomosc
                break;
            }
        }
        if(j == -1){
            fprintf(stderr, "unknown pid: %d\n", pid);
            exit(EXIT_FAILURE);
        }
        
        int students_stage = current_stage[j]; //aktualny stage studenta
        int d = stage_points[students_stage - 1] + (rand() % 20) + 1; //stage difficulty

        if(attempt_score >= d){
            printf("Teacher: Student %d finished stage %d\n", pid, students_stage);
            if(write(teacher_pipes[2*j + 1], &passed, sizeof(int)) < 0) ERR("write");
            current_stage[j]++;
            if(current_stage[j] == 5) finished_students++;
        } else {
            printf("Teacher: Student %d needs to fix stage %d\n", pid, students_stage);
            if(write(teacher_pipes[2*j + 1], &failed, sizeof(int)) < 0) ERR("write");
        }
    }
    if(stop){
        printf("Teacher: END OF TIME!\n");
        for(int i = 0; i < n; i++){
            printf("student %d pid %d finished at stage %d\n", i + 1, pids[i], current_stage[i]);
        }
    }

    if(close(students_pipe[0]) < 0) ERR("close");
    for(int i = 0; i < n; i++){
        if(close(teacher_pipes[2*i + 1]) < 0) ERR("close");
    }

    while(wait(NULL) > 0);
    if(!stop && finished_students == n) printf("Teacher: IT'S FINALLY OVER!\n");
    free(teacher_pipes);
    free(pids);
    return EXIT_SUCCESS;
}
