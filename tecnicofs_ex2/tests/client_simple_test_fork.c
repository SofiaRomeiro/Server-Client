#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */

int main(int argc, char **argv) {

    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    int f, state;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    int pid1 = fork();
    if (pid1 == 0) {
        assert(tfs_mount(argv[1], argv[2]) == 0);
        exit(0);
    } else {
        pid1 = wait(&state);
    }

    int pid2 = fork();
    if (pid2 == 0) {
        assert(tfs_mount(argv[1], argv[2]) == 0);
        exit(0);
    } else {
        pid2 = wait(&state);
    }


    printf("Mount concluded with success!\n");

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);


    r = tfs_write(f, str, strlen(str));
    assert(r == strlen(str));


    assert(tfs_close(f) != -1);


    f = tfs_open(path, 0);
    assert(f != -1);


    r = tfs_read(f, buffer, sizeof(buffer) - 1);

    //printf("r is %ld\n", r);

    assert(r == strlen(str));

    buffer[r] = '\0';

    assert(strcmp(buffer, str) == 0);

    assert(tfs_close(f) != -1);

    assert(tfs_unmount() == 0);

    printf("Successful test.\n");

    return 0;
}
