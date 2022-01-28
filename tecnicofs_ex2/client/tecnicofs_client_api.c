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

static int debug = 0;

static int fcli;
static int fserv;
static int session_id = -1;

void print_debug(char *str) {
    if (debug) printf("%s\n", str);
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    ssize_t n;

    char buffer[4 + BUFFER_SIZE + 1];
    memset(buffer, '\0', sizeof(buffer));

     // named pipe - server
    if ((fserv = open(server_pipe_path, O_WRONLY)) < 0) {
        printf("[INFO - tfs_mount] Error opening server: %s\n", strerror(errno));
        return -1;
    }

    size_t str_len = strlen(client_pipe_path);

    char code = TFS_OP_CODE_MOUNT + '0';

    memcpy(buffer, &code, sizeof(char));

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

    while (1) {
        n = read(fcli, buffer, sizeof(int));

        if (n == 0) {
            printf("[INFO - client_api] EOF\n");
            break;
        }            

        else if (n == -1) {
            printf("[ERROR - client_api] Error reading file\n");
            return -1;
        }
    }      

    session_id = atoi(buffer); 

    return session_id;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */

    size_t pos = 0;
    
    char buffer[100];
    memset(buffer, '\0', sizeof(buffer));

    char name_s[BUFFER_SIZE];
    memset(name_s, '\0', sizeof(name_s));
    memcpy(name_s, name, sizeof(name_s));

    char code = TFS_OP_CODE_OPEN + '0';
    memcpy(buffer, &code, sizeof(char));  

    char session_id_s[10];
    memset(session_id_s, '\0', sizeof(session_id_s));

    sprintf(session_id_s, "%d", session_id);
    memcpy(buffer + 1, session_id_s, sizeof(int));
    
    memcpy(buffer + 5, name_s, sizeof(name_s));

    char flags_c = (char) (flags + '0');
    memcpy(buffer + 45, &flags_c, sizeof(char));
    pos += sizeof(int);

    // named pipe - client
    if (fserv  < 0) {
        printf("[tfs_open] Error opening client\n");
        return -1;
    }

    ssize_t size_written = write(fserv, buffer, sizeof(buffer));

    if (size_written < sizeof(buffer)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    memset(buffer, '\0', sizeof(buffer));

    while (1) {
        ssize_t read_size = read(fcli, buffer, sizeof(char));

        if (read_size == 0) {
            printf("[INFO - client_api] EOF\n");
            break;
        }            

        else if (read_size == -1) {
            printf("[ERROR - client_api] Error reading file\n");
            return -1;
        }
    }

    int fhandler = atoi(buffer);

    if (fhandler < 0) return -1;

    return fhandler;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    printf("Fhandle : %d\n", fhandle);
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    printf("Fhandle : %d\nbuffer : %p\nlen : %ld\n", fhandle, buffer, len);
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
