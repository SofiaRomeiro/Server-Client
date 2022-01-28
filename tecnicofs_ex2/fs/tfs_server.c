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
    char name[40];
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

    open_sessions++;

    free = find_free_pos();

    session_t *session = &(sessions[free]);

    size_t len = strlen(name); //sem '\0'

    memset(session->name, '\0', sizeof(session->name));

    memcpy(session->name, name, len);
    
    session->session_id = free;

    if ((fcli = open(name, O_WRONLY)) < 0) {
        printf("[ tfs_server ] tfs_mount : Failed to open client pipe\n");
        return -1;
    }

    char session_id_cli[sizeof(int)];
    memset(session_id_cli, '\0', sizeof(session_id_cli));

    memcpy(session_id_cli, (char *)&free, sizeof(int));

    n = write(fcli, session_id_cli, sizeof(char));

    if (n == -1) {
        printf("[ ERROR ] Reading : Failed\n");
        return -1;
    }

    if (close(fcli) == -1) {
        printf("[ERROR - tfs_server] Error closing client\n");
        return -1;
    }

    return 0;

}

int main(int argc, char **argv) {

    /* TO DO */

    int fserv;
    ssize_t n;
    int command;
    char *server_pipe;
    char buffer[SIZE + 1];
    char aux_buffer[SIZE + 1];
    char name[CLI_PIPE_SIZE + 1];

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
    
    /*
    if ((fserv = open(server_pipe, O_RDONLY)) < 0)  {
        printf("[tfs_server] Open failed : %s\n", strerror(errno));
        return -1;
    }*/
        
    assert(tfs_init() != -1);

    // ----------------------------------------- START RESPONDING TO REQUESTS --------------------------------------

    while (1) {

        memset(buffer, '\0', SIZE);

        if ((fserv = open(server_pipe, O_RDONLY)) < 0) 
                return -1;

        n = read(fserv, buffer, sizeof(char));

        printf("[INFO - SERVER] n = %ld\n", n);

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

        printf("[INFO - SERVER] buffer : %s\n", buffer);

        command = atoi(buffer);

        switch(command) {
            case (TFS_OP_CODE_MOUNT):

                printf("Calling mount...\n");

                if (tfs_handle_mount(name, fserv) == -1) {
                    printf("[ tfs_server ] Error while mounting\n");
                    return -1;
                }

                close(fserv);

            break;

            case (TFS_OP_CODE_UNMOUNT):

                if (open_sessions == 0) {
                    printf("[ tfs_server ] tfs_mount: There are no open sessions, please open one before unmount\n");
                    return -1;
                }
                open_sessions--;

                // ir buscar a pipe do client com o session_id e responder através dela (assim sabe-se
                // que sessão terminar

            break;

            case (TFS_OP_CODE_OPEN):

                memset(name, '\0', sizeof(name));
                memset(buffer, '\0', sizeof(buffer));
                memset(aux_buffer, '\0', sizeof(aux_buffer));

                ssize_t size_read = read(fserv, buffer, SIZE);
                if (size_read == -1) return -1;

                // session_id
                memcpy(aux_buffer, buffer, sizeof(int));

                int session_id = atoi(aux_buffer);  
              
                // name
                memcpy(name, buffer + 4, NAME_PIPE_SIZE);
                memset(aux_buffer, '\0', sizeof(aux_buffer));

                // flags
                memcpy(aux_buffer, buffer + 44, sizeof(int));
                int flags = atoi(aux_buffer);

                printf("[INFO - SERVER] Session_id = %d, Flags = %d\nName: |%s|\n", session_id, flags, name);

                int fhandler = tfs_open(name, flags);
                if (fhandler == -1) return -1;

                printf("[INFO - SERVER] open = %d\n", fhandler);

                char fhandler_c = (char) (fhandler + '0');

                int fcli = open(sessions[session_id].name, O_WRONLY);

                if (fcli < 0) return -1;

                printf("[INFO - SERVER] fhandler_c : |%c|\n", fhandler_c);

                ssize_t write_size = write(fcli, &fhandler_c, sizeof(char));

                if (write_size < 0) return -1;

                // ...

            break;

            case (TFS_OP_CODE_CLOSE):
            break;

            case (TFS_OP_CODE_WRITE):
            break;

            case (TFS_OP_CODE_READ):
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