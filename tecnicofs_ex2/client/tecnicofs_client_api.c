#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define PERMISSIONS 0777
#define BUFFER_SIZE 40

static int session_id = -1;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    int fcli, fserv;
    ssize_t n;
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', sizeof(buffer));

     // named pipe - server
    if ((fserv = open(server_pipe_path, O_WRONLY | O_NONBLOCK)) < 0) {
        printf("[INFO - tfs_mount] Error opening server: %s\n", strerror(errno));
        return -1;
    }

    size_t str_len = strlen(client_pipe_path);

    sprintf(buffer, "%d", TFS_OP_CODE_MOUNT);

    memcpy(buffer + 1, client_pipe_path, str_len);

    if (write(fserv, buffer, sizeof(buffer)) == -1) {
        printf("[tfs_mount] Error writing\n");
        return -1;
    }        

    memset(buffer, '\0', sizeof(buffer));

    // abre a pipe para o servidor 
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", client_pipe_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(client_pipe_path, PERMISSIONS) != 0) { // criar named pipe - client
        printf("[tfs_mount] mkfifo failed\n");
        return -1;
    }

    // named pipe - client
    if ((fcli = open(client_pipe_path, O_RDONLY)) < 0) {
        printf("[tfs_mount] Error opening client\n");
        return -1;
    } 

    printf("[client_api] Reading from server...\n");       

    if ((n = read(fcli, buffer, sizeof(int))) == -1) {
        printf("[tfs_mount] Error reading\n");
        return -1;
    } 

    printf("[api_client] n is %ld\n", n);       

    printf("[client_api] Finish reading!\n");

    session_id = atoi(buffer); 

    printf("[ + ] Session_id = %d\n", session_id);

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    printf("Name : %s\nFlags : %d", name, flags);

    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    printf("Fhandle : %d\n", fhandle);
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    printf("Fhandle : %d\n, buffer : %p\nlen : %ld\n", fhandle, buffer, len);
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    printf("Fhandle : %d\n, buffer : %p\nlen : %ld\n", fhandle, buffer, len);
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
