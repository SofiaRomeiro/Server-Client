<<<<<<< HEAD
=======

>>>>>>> dev-sofia
#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

/*  
    WARNING : Set S to 3 before testing
<<<<<<< HEAD

=======
>>>>>>> dev-sofia
    1 tifu, 2 tifu, QUANTOS TIFU DEU?!
*/

int main(int argc, char **argv) {

    char *str = "AAA!";
    char *path1 = "/f1";
    char *path2 = "/f2";
    char *path3 = "/f3";
    char *path4 = "/f4";

    char buffer[40];

    int f, state, pid1, pid2, pid3, pid4;
    ssize_t r;

    if (argc < 6) {
        printf("You must provide the following arguments: 'client_pipe_path',\
         'client_pipe_path' and 'server_pipe_path'\n");
        return 1;
    }


    pid1 = fork();

    if (pid1 == 0) {
        assert(tfs_mount(argv[1], argv[5]) == 0);
        f = tfs_open(path1, TFS_O_CREAT);
        assert(f != -1);
        assert(tfs_close(f) != -1);
        f = tfs_open(path1, 0);
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
        assert(tfs_mount(argv[2], argv[5]) == 0);
        f = tfs_open(path2, TFS_O_CREAT);
        assert(f != -1);
        assert(tfs_close(f) != -1);
        assert(tfs_unmount() == 0);
        exit(0);
    } 
    else if (pid2 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    pid3 = fork();
    if (pid3 == 0) {
        assert(tfs_mount(argv[3], argv[5]) == 0);
        f = tfs_open(path3, TFS_O_CREAT);
        assert(f != -1);
        r = tfs_write(f, str, strlen(str));
        assert(r == strlen(str));
        assert(tfs_close(f) != -1);
<<<<<<< HEAD
        //assert(tfs_shutdown_after_all_closed() == 0);
=======
>>>>>>> dev-sofia
        assert(tfs_shutdown_after_all_closed() == 0);
        exit(0);
    }
    else if (pid3 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

<<<<<<< HEAD
=======
    //sleep(1);

>>>>>>> dev-sofia
    pid4 = fork();
    if (pid4 == 0) {
        assert(tfs_mount(argv[4], argv[5]) != 0);
        f = tfs_open(path4, TFS_O_CREAT);
        assert(f == -1);
        r = tfs_write(f, str, strlen(str));
        assert(r == strlen(str));
        assert(tfs_close(f) == -1);
        f = tfs_open(path4, 0);
        assert(f == -1);
        r = tfs_read(f, buffer, sizeof(buffer) - 1);
        assert(r != strlen(str));
        buffer[r] = '\0';
        assert(strcmp(buffer, str) != 0);
        assert(tfs_close(f) == -1);
        assert(tfs_unmount() == 0);
        exit(0);
    } 
    else if (pid4 < 0) {
        printf("Error forking: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
  
    pid1 = wait(&state);
    printf("[TEST] Wait 1\n");
    pid2 = wait(&state);
    printf("[TEST] Wait 2\n");
    pid3 = wait(&state);
    printf("[TEST] Wait 3\n");
    pid4 = wait(&state);
    printf("[TEST] Wait 4\n");

    assert(pid1 != -1);
    assert(pid2 != -1);
    assert(pid3 != -1);
    assert(pid4 != -1);
    
    printf("Successful test.\n");

    return 0;
}