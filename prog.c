#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char *argv){
    printf("usage %s: fifo file\n", argv[0]);
    exit(EXIT_FAILURE);
}

void read_from_fifo(int fd){
    ssize_t count;
    char c;
    do{
        if((count = read(fd, &c, 1)) < 0) ERR("read");
        if(count > 0 && isalnum(c)) printf("%c", c);
    } while (count > 0);
}

int main(int argc, char *argv){
    int fd;
    if(argc != 2) usage(argc, argv);
    if(mkfifio(argv[1], S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) < 0){ //makes fifo named as the file given in first argument, read/write persmissions for owner and read for group and others
        if(errno != EEXIST) ERR("create fifo");
    }
    if(fd = open(argv[1], O_RDONLY) < 0) ERR("open");
    read_from_fifo(fd);
    if(close(fd) < 0) ERR("close fifo");
    return EXIT_SUCCESS;
}