#include "tecnicofs_client_api.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PERMISSIONS 0777
#define BUFFER_SIZE 40

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    int fcli, fserv;
    char buffer[BUFFER_SIZE];

    // abre a pipe para o servidor 
    unlink(client_pipe_path);

    if (mkfifo(client_pipe_path, PERMISSIONS) < 0) {
        return -1;
    }

    if ((fcli = open(client_pipe_path, O_RDONLY)) < 0) 
        return -1;

    if ((fserv = open(server_pipe_path, O_WRONLY)) < 0)
        return -1;

    memset(buffer, '\0', sizeof(buffer));
    // manda lhe o op code = 1
    memcpy(buffer, (char *)TFS_OP_CODE_MOUNT, sizeof(char));
    // manda lhe o nome do client_pipe 
    size_t str_len = strlen(client_pipe_path);
    memcpy(buffer + 1, client_pipe_path, str_len);

    // recebe 0 => sucesso, se nao -1

    return -1;
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
