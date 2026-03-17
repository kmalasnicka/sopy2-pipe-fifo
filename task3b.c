#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char **argv){
    fprintf(stderr, "usage: %s 3<=n<=20", argv[0]);
    exit(EXIT_FAILURE);
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

    if(close(students_pipe[0]) < 0) ERR("close");
    for(int i = 0; i < n; i++){
        if(close(teacher_pipes[2*i + 1]) < 0) ERR("close");
    }

    while(wait(NULL) > 0);
    free(teacher_pipes);
    free(pids);
    return EXIT_SUCCESS;
}