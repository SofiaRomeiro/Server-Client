#include "operations.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#define SIZE 99

#define PERMISSIONS 0777
#define CLI_PIPE_SIZE 39
#define S 20
#define SIZE_OF_CHAR 1
#define NAME_PIPE_SIZE 40

typedef struct {
    int session_id;
    int fhandler;
} session_t;

typedef enum {FREE_POS = 1, TAKEN_POS = 0} session_state_t;

static int open_sessions;
static session_t sessions[S];
static session_state_t free_sessions[S];

static int debug = 0;

void print_debug(char *str) {
    if (debug) printf("%s\n", str);
}

int find_free_pos() {

    int free = -1;
    
    for (int i = 0; i < S; i++) {
        if (free_sessions[i] == FREE_POS) {            
            free = i;
            break;
        }
    }

    if (free == -1) {
        printf("[ tfs_server ] All positions occupied\n");
        exit(EXIT_FAILURE);
    } 

    free_sessions[free] = TAKEN_POS;
    
    return free;
}

int tfs_handle_mount(char name[], int fserv) {

    int fcli;
    int free = 0;

    memset(name, '\0', CLI_PIPE_SIZE + 1);
    
    ssize_t n = read(fserv, name, NAME_PIPE_SIZE);

    if (n == -1) {
        printf("[ ERROR ] Reading : Failed\n");
        return -1;
    }

    if (open_sessions == S) {
        printf("[ tfs_server ] tfs_mount: Reached limit number of sessions, please wait\n");
        return -1;
    }

    if ((fcli = open(name, O_WRONLY)) < 0) {
            printf("[ tfs_server ] tfs_mount : Failed to open client pipe\n");
            return -1;
    }

    open_sessions++;

    free = find_free_pos();

    session_t *session = &(sessions[free]);
    session->session_id = free;

    session->fhandler = fcli;

    char session_id_cli[sizeof(int)];
    memset(session_id_cli, '\0', sizeof(session_id_cli));
    sprintf(session_id_cli, "%d", free);

    printf("[INFO - SERVER] Pipe is open\n");

    n = write(fcli, session_id_cli, sizeof(int));

    if (n == -1) {
        printf("[ ERROR ] Reading : Failed\n");
        return -1;
    }

    return 0;

}

void tfs_handle_read(int fserv) {
    char buffer[100];
    char aux_buffer[100];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    int session_id = atoi(buffer);

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    int fhandle = atoi(buffer);

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    size_t len = (size_t) atoi(buffer);

    char read_b[sizeof(char) * len + 1]; 
    memset(read_b, '\0', sizeof(read_b));

    if (read(fserv, buffer, len) == -1) {
        free(read_b);
        exit(EXIT_FAILURE);
    }

    ssize_t read_bytes = tfs_read(fhandle, read_b, len);

    int fcli = sessions[session_id].fhandler;
    if (fcli < 0) {
        exit(EXIT_FAILURE);
    }

    if (read_bytes < 0)  {
        memset(buffer, '\0', sizeof(buffer));
        sprintf(buffer, "%d", (int)read_bytes);

        ssize_t write_size = write(fcli, buffer, sizeof(int));

        if (write_size < 0) exit(EXIT_FAILURE);
        if(close(fcli) == -1) exit(EXIT_FAILURE);
        return;
    }

    memset(buffer, '\0', sizeof(buffer));

    char send[sizeof(int) + (sizeof(char) * len) + 1];
    memset(send, '\0', sizeof(send));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    sprintf(aux_buffer, "%d", (int)read_bytes);
    memcpy(send, aux_buffer, sizeof(int));
    memcpy(send + 4, read_b, (size_t)read_bytes);

    ssize_t write_size = write(fcli, send, sizeof(int) + len);

    if (write_size < 0) exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {

    /* TO DO */

    int fserv, fcli, fhandle;
    ssize_t n, write_size;
    int command;
    char *server_pipe;
    char buffer[SIZE + 1];
    char aux_buffer[SIZE + 1];
    char name[CLI_PIPE_SIZE + 1];
    int session_id;

    for (int i = 0; i < S; i++) {
        free_sessions[i] = FREE_POS;
    }

    open_sessions = 0;
    memset(buffer, '\0', SIZE + 1);
    memset(name, '\0', CLI_PIPE_SIZE + 1);    

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        exit(EXIT_FAILURE);
    }

    server_pipe = argv[1];

    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe);

    if (unlink(server_pipe) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(server_pipe, PERMISSIONS) != 0) {
        printf("[tfs_server] mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
        
    assert(tfs_init() != -1);

    // ----------------------------------------- START RESPONDING TO REQUESTS --------------------------------------

    while (1) {

        memset(buffer, '\0', SIZE);

        if ((fserv = open(server_pipe, O_RDONLY)) < 0) 
                return -1;

        n = read(fserv, buffer, sizeof(char));

        if (n == 0) { //EOF
            close(fserv);
            if ((fserv = open(server_pipe, O_RDONLY)) < 0) 
                return -1;
            continue;            
        }

        if (n == -1) {
            printf("[ ERROR ] Reading : Failed\n");
            return -1;
        }

        command = atoi(buffer);

        switch(command) {
            case (TFS_OP_CODE_MOUNT):

                printf("[INFO - SERVER] Calling mount...\n");

                if (tfs_handle_mount(name, fserv) == -1) {
                    printf("[ tfs_server ] Error while mounting\n");
                    return -1;
                }

                close(fserv);

            break;

            case (TFS_OP_CODE_UNMOUNT):

                printf("[INFO - SERVER] Calling unmount...\n");

                if (open_sessions == 0) {
                    printf("[ tfs_server ] tfs_mount: There are no open sessions, please open one before unmount\n");
                    return -1;
                }
                open_sessions--;

                ssize_t ssid = read(fserv, buffer, sizeof(int));
                if (ssid == -1) return -1;

                // session_id
                memcpy(aux_buffer, buffer, sizeof(int));

                session_id = atoi(aux_buffer);  

                if (close(fserv) == -1) return -1;
                if (close(sessions[session_id].fhandler) == -1) return -1;

                sessions[session_id].fhandler = -1;
                sessions[session_id].session_id = -1;

                // ir buscar a pipe do client com o session_id e responder através dela (assim sabe-se
                // que sessão terminar

            break;

            case (TFS_OP_CODE_OPEN):
                printf("[INFO - SERVER] Calling open...\n");

                memset(name, '\0', sizeof(name));
                memset(buffer, '\0', sizeof(buffer));
                memset(aux_buffer, '\0', sizeof(aux_buffer));

                ssize_t size_read = read(fserv, buffer, SIZE);
                if (size_read == -1) return -1;

                // session_id
                memcpy(aux_buffer, buffer, sizeof(int));

                session_id = atoi(aux_buffer);  
              
                // name
                memcpy(name, buffer + 4, NAME_PIPE_SIZE);
                memset(aux_buffer, '\0', sizeof(aux_buffer));

                // flags
                memcpy(aux_buffer, buffer + 44, sizeof(int));
                int flags = atoi(aux_buffer);

                int fhandler = tfs_open(name, flags);
                if (fhandler == -1) return -1;

                char fhandler_c = (char) (fhandler + '0');

                fcli = sessions[session_id].fhandler;

                write_size = write(fcli, &fhandler_c, sizeof(char));

                if (write_size < 0) return -1;

                // ...

            break;

            case (TFS_OP_CODE_CLOSE):
                printf("[INFO - SERVER] Calling close...\n");

                if (read(fserv, buffer, sizeof(int)) == -1) return -1;
                session_id = atoi(buffer);

                if (read(fserv, buffer, sizeof(int)) == -1) return -1;
                fhandle = atoi(buffer);

                int fclose = tfs_close(fhandle);

                if (fclose < 0) return -1;

                fcli = sessions[session_id].fhandler;

                memset(buffer, '\0', sizeof(buffer));
                sprintf(buffer, "%d", fclose);

                write_size = write(fcli, buffer, sizeof(int));

                if (write_size < 0) return -1;           

            break;

            case (TFS_OP_CODE_WRITE):

                printf("[INFO - SERVER] : Calling write...\n");

                if (read(fserv, buffer, sizeof(int)) == -1) return -1;
                session_id = atoi(buffer);

                if (read(fserv, buffer, sizeof(int)) == -1) return -1;
                fhandle = atoi(buffer);

                if (read(fserv, buffer, sizeof(int)) == -1) return -1;
                size_t len = (size_t) atoi(buffer);
 
                memset(buffer, '\0', len);
                if (read(fserv, buffer, len) == -1) return -1;

                ssize_t written = tfs_write(fhandle, buffer, len);

                if (written < 0 ) return -1;

                fcli = sessions[session_id].fhandler;
                if (fcli < 0) return -1;

                memset(buffer, '\0', sizeof(buffer));
                sprintf(buffer, "%d", (int)written);

                write_size = write(fcli, buffer, sizeof(int));

                if (write_size < 0) return -1;

            break;

            case (TFS_OP_CODE_READ):

                printf("[INFO - SERVER] : Calling read...\n");

                tfs_handle_read(fserv);              

            break;

            case (TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED):
            break;

            default:
                printf("[tfs_server] Switch case : Command %c: No correspondance\n", command);
                //return -1;
            break;


        }
    }

    

    return 0;
}