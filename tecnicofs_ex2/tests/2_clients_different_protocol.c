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

    int f, state, pid1, pid2;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path',\
         'client_pipe_path' and 'server_pipe_path'\n");
        return 1;
    }

    pid1 = fork();

    if (pid1 == 0) {
        assert(tfs_mount(argv[1], argv[3]) == 0);
        f = tfs_open(path, TFS_O_CREAT);
        assert(tfs_close(f) != -1);
        f = tfs_open(path, 0);
        assert(f != -1);
        assert(f != -1);
        r = tfs_write(f, str, strlen(str));
        assert(r == strlen(str));;
        assert(tfs_close(f) != -1);
        assert(tfs_unmount() == 0);
        exit(0);
    } 
    else if (pid1 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (pid2 == 0) {
        assert(tfs_mount(argv[2], argv[3]) == 0);
        f = tfs_open(path, TFS_O_CREAT);
        assert(f != -1);
        r = tfs_write(f, str, strlen(str));
        assert(r == strlen(str));
        assert(tfs_close(f) != -1);
        f = tfs_open(path, 0);
        assert(f != -1);
        r = tfs_read(f, buffer, sizeof(buffer) - 1);
        assert(r == strlen(str));
        buffer[r] = '\0';
        assert(strcmp(buffer, str) == 0);
        assert(tfs_close(f) != -1);
        assert(tfs_unmount() == 0);
        exit(0);
    } 
    else if (pid2 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    pid1 = wait(&state);
    pid2 = wait(&state);

    assert(pid1 != -1);
    assert(pid2 != -1);
    
    printf("Successful test.\n");

    return 0;
}
