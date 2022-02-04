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

    int state, pid1, pid2, pid3, pid4, pid5;

    printf("\nTo pass this test, one assert should \
fail!\n ===> Don't forget to set 'S' to 4 <===\n\n");

    if (argc < 7) {
        printf("You must provide the following arguments: 'client_pipe_path',\
         'client_pipe_path' and 'server_pipe_path'\n");
        return 1;
    }

    pid1 = fork();

    if (pid1 == 0) {
        assert(tfs_mount(argv[1], argv[6]) == 0);
        exit(0);
    } 
    else if (pid1 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (pid2 == 0) {
        assert(tfs_mount(argv[2], argv[6]) == 0);
        exit(0);
    } 
    else if (pid2 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid3 = fork();
    if (pid3 == 0) {
        assert(tfs_mount(argv[3], argv[6]) == 0);
        exit(0);
    } 
    else if (pid3 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid4 = fork();
    if (pid4 == 0) {
        assert(tfs_mount(argv[4], argv[6]) == 0);
        exit(0);
    } 
    else if (pid4 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid5 = fork();
    if (pid5 == 0) {
        assert(tfs_mount(argv[5], argv[6]) == 0);
        exit(0);
    } 
    else if (pid5 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
  
    pid1 = wait(&state);
    pid2 = wait(&state);
    pid3 = wait(&state);
    pid4 = wait(&state);
    pid5 = wait(&state);

    assert(pid1 != -1);
    assert(pid2 != -1);
    assert(pid3 != -1);
    assert(pid4 != -1);
    assert(pid5 != -1);
    
    printf("Successful test.\n");

    return 0;
}
