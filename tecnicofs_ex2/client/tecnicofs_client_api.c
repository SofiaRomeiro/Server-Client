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

/*
Sleep + Wait
*/
void slait(char *buffer_c, size_t len) {

    ssize_t written_count = 0;

    while(1) {

        ssize_t written_tfs = read(fcli, buffer_c + written_count, len);        

        if (written_tfs == -1) {
            printf("[ERROR - client_api] Error reading file\n");
            exit(EXIT_FAILURE);
        }

        else if (written_tfs == 0) {
            printf("[INFO - API] Slait EOF\n");
            return;
        }

        written_count += written_tfs;

        if (written_count >= len)
            break;
    }
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

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

    printf("[INFO - API] Pipe is open\n");

    slait(buffer, sizeof(int));   

    printf("[INFO - API] Slait called\n");

    session_id = atoi(buffer); 

    return session_id;
}

int tfs_unmount() {
    
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

    slait(buffer, sizeof(char));

    int fhandler = atoi(buffer);

    if (fhandler < 0) return -1;

    return fhandler;
}

int tfs_close(int fhandle) {

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

    slait(aux, sizeof(int));

    int close = atoi(buffer);

    if (close < 0) return -1;

    return close;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    printf("Fhandle : %d\nbuffer : %p\nlen : %ld\n", fhandle, buffer, len);
    
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

    slait(buffer_c, len);

    int written = atoi(buffer_c);

    if (written < 0) return -1;

    return written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */

    printf("[INFO - API] Calling read...\n");

    size_t buffer_size = 1 + 4 + 4 + 4 + 1;
    char buffer_c[buffer_size];
    memset(buffer_c, '\0', sizeof(buffer));
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
    if (fserv  < 0) {
        printf("[tfs_open] Error opening client\n");
        return -1;
    }

    ssize_t size_written = write(fserv, buffer_c, sizeof(buffer_c));

    if (size_written < sizeof(buffer_c)) {
        printf("[ERROR - API] tfs_open error on writing");
    }

    memset(buffer_c, '\0', sizeof(buffer_c));

    slait(buffer_c, sizeof(int) + len);

    memset(aux, '\0', sizeof(aux));
    memcpy(aux, buffer_c, sizeof(int));

    int read_code = atoi(aux);

    memcpy(buffer, buffer_c + sizeof(int), (size_t)read_code);

    // READ ANSWER
    return read_code;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
