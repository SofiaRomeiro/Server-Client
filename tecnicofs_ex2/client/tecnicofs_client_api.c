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

    printf("[INFO - API] Calling api mount...\n");

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

    if (write(fserv, buffer, strlen(buffer)) == -1) {
        printf("[tfs_mount] Error writing\n");
        return -1;
    }        

    memset(buffer, '\0', sizeof(buffer));

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

    slait(buffer, sizeof(int), fcli);   

    session_id = atoi(buffer); 

    if (session_id < 0) return -1;

    return 0;
}

int tfs_unmount() {

    printf("[INFO - API] Calling api unmount...\n");
    
    char buffer[10];
    char aux[10];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    // OP_CODE

    char code = TFS_OP_CODE_UNMOUNT + '0';
    memcpy(buffer, &code, sizeof(char));  

    // SESSION_ID

    sprintf(aux, "%d", session_id);
    memcpy(buffer + 1, aux, sizeof(int));

    // SEND MSG TO SERVER

    ssize_t size_written = write(fserv, buffer, sizeof(buffer));

    if (size_written < sizeof(buffer)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    if (close(fserv) == -1) return -1;
    if (close(fcli) == -1) return -1;

    return 0;  
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */

    printf("[INFO - API] Calling api open...\n");


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

    if (fserv < 0) {
        printf("[tfs_open] Error opening client\n");
        return -1;
    }

    // arranjar forma de contar os bytes a enviar para nao enviar lixo
    ssize_t size_written = write(fserv, buffer, sizeof(buffer));

    if (size_written < sizeof(buffer)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    memset(buffer, '\0', sizeof(buffer));

    slait(buffer, sizeof(char), fcli);

    int fhandler = atoi(buffer);

    if (fhandler < 0) return -1;

    return fhandler;
}

int tfs_close(int fhandle) {

    printf("[INFO - API] Calling api close...\n");

    char buffer[10];
    char aux[10];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    // OP_CODE

    char code = TFS_OP_CODE_CLOSE + '0';
    memcpy(buffer, &code, sizeof(char));  

    // SESSION_ID

    sprintf(aux, "%d", session_id);
    memcpy(buffer + 1, aux, sizeof(int));

    // FHANDLE

    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%d", fhandle);
    memcpy(buffer + 5, aux, sizeof(int));  

    // SEND MESSAGE
    if (fserv  < 0) {
        printf("[tfs_open] Error opening client\n");
        return -1;
    }

    ssize_t size_written = write(fserv, buffer, sizeof(buffer));

    if (size_written < sizeof(buffer)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    // READ ANSWER

    memset(aux, '\0', sizeof(aux));

    slait(aux, sizeof(int), fcli);

    int close = atoi(aux);

    if (close < 0) return -1;

    return close;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    printf("[INFO - API] Calling api write...\n");
    
    size_t buffer_size = 1 + 4 + 4 + 4 + len + 1;
    char buffer_c[buffer_size];
    memset(buffer_c, '\0', sizeof(buffer));
    char aux[10];
    memset(aux, '\0', sizeof(aux));

    // OP_CODE

    char code = TFS_OP_CODE_WRITE + '0';
    memcpy(buffer_c, &code, sizeof(char));  

    // SESSION_ID

    sprintf(aux, "%d", session_id);
    memcpy(buffer_c + 1, aux, sizeof(int));

    // FHANDLE
    
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%d", fhandle);
    memcpy(buffer_c + 5, aux, sizeof(int));

    // LEN

    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%ld", len);
    memcpy(buffer_c + 9, aux, sizeof(int));

    // BUFFER WITH CONTENT

    memcpy(buffer_c + 13, buffer, len);

    // SEND MSG TO SERVER
    if (fserv  < 0) {
        printf("[tfs_open] Error opening client\n");
        return -1;
    }

    ssize_t size_written = write(fserv, buffer_c, sizeof(buffer_c));

    if (size_written < sizeof(buffer_c)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    memset(buffer_c, '\0', sizeof(buffer_c));

    slait(buffer_c, len, fcli);

    int written = atoi(buffer_c);

    if (written < 0) return -1;

    return written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    printf("[INFO - API] Calling api read...\n");

    size_t buffer_size = 1 + 4 + 4 + 4 + 1;
    char buffer_c[buffer_size];
    memset(buffer_c, '\0', sizeof(buffer_c));
    char aux[10];
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_READ + '0';
    memcpy(buffer_c, &code, sizeof(char)); 

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer_c + 1, aux, sizeof(int));

    // FHANDLE
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%d", fhandle);
    memcpy(buffer_c + 5, aux, sizeof(int));

    // LEN
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%ld", len);
    memcpy(buffer_c + 9, aux, sizeof(int));

    // SEND MSG TO SERVER

    ssize_t size_written = write(fserv, buffer_c, sizeof(buffer_c));

    if (size_written < sizeof(buffer_c)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    memset(buffer_c, '\0', sizeof(buffer_c));

    slait(buffer_c, sizeof(int), fcli);

    memset(aux, '\0', sizeof(aux));
    memcpy(aux, buffer_c, sizeof(int));

    size_t read_code = (size_t) atoi(aux);

    memset(buffer_c, '\0', sizeof(buffer_c));
    slait(buffer_c, read_code, fcli);

    memcpy(buffer, buffer_c, read_code);

    // READ ANSWER
    return (ssize_t)read_code;
}

int tfs_shutdown_after_all_closed() {

    char buffer[5];
    memset(buffer, '\0', sizeof(buffer));
    char aux[4];
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_READ + '0';
    memcpy(buffer, &code, sizeof(char)); 

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer + 1, aux, sizeof(int));

    ssize_t size_written = write(fserv, buffer, sizeof(buffer));

    if (size_written < sizeof(buffer)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    memset(aux, '\0', sizeof(aux));

    slait(aux, sizeof(int), fcli);

    int ret = atoi(aux);

    if (ret < 0) return -1;


    return -1;
}
