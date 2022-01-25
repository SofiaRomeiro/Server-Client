#include "operations.h"
#include "client/tecnicofs_client_api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define SIZE 99

#define PERMISSIONS 0777
#define CLI_PIPE_SIZE 39
#define S 20
#define SIZE_OF_CHAR 1
#define NAME_PIPE_SIZE 40

typedef struct {
    int session_id;
    char pipe_name[40];
} session_t;

typedef enum {FREE_POS = 1, TAKEN_POS = 0} session_state_t;

static int open_sessions;
static session_t sessions[S];
static session_state_t free_sessions[S];


int find_free_pos() {

    int free = -1;
    
    for (int i = 0; i < S; i++) {
        if (free_sessions[i] == FREE_POS) {            
            free = i;
            break;
        }
    }

    if (free == -1) {
        //printf("[ tfs_server ] All positions occupied\n");
        exit(EXIT_FAILURE);
    } 

    free_sessions[free] = TAKEN_POS;

    //printf("[tfs_server] free pos = %d\n", free);
    
    return free;
}

int tfs_handle_mount(char pipe_name[], int fserv) {

    int fcli;
    int free = 0;

    memset(pipe_name, '\0', CLI_PIPE_SIZE + 1);

    //printf("[tfs_handle_mount] Reading...\n");
    
    ssize_t n = read(fserv, pipe_name, NAME_PIPE_SIZE);

    if (n == -1) {
        //printf("[ ERROR ] Reading : Failed\n");
        return -1;
    }

    if (open_sessions == S) {
        //printf("[ tfs_server ] tfs_mount: Reached limit number of sessions, please wait\n");
        return -1;
    }

    open_sessions++;

    free = find_free_pos();

    session_t *session = &(sessions[free]);

    size_t len = strlen(pipe_name); //sem '\0'

    memset(session->pipe_name, '\0', sizeof(session->pipe_name));

    memcpy(session->pipe_name, pipe_name, len);

    // choose session id
    
    session->session_id = free;

    // enviar ao client o session id

    if ((fcli = open(pipe_name, O_WRONLY) < 0)) {
        //printf("[ tfs_server ] tfs_mount : Failed to open client pipe\n");
        return -1;
    }

    char session_id_cli[sizeof(char) + 1];
    memset(session_id_cli, '\0', sizeof(session_id_cli));
    //sprintf(session_id_cli, "%d", free);k

    session_id_cli[0] = (char) free + '0';

    n = 0;

    n = write(fcli, session_id_cli, sizeof(char));

    printf("[tfs_server] Write to client... n is %ld\n", n);

    if (n == -1) {
        //printf("[ ERROR ] Reading : Failed\n");
        return -1;
    }

    return 0;

}

int main(int argc, char **argv) {

    int fserv, fcli;
    ssize_t n;
    int command;
    char *server_pipe;
    char buffer[SIZE + 1];;
    char pipe_name[CLI_PIPE_SIZE + 1];

    for (int i = 0; i < S; i++) {
        free_sessions[i] = FREE_POS;
    }

    open_sessions = 0;
    memset(buffer, '\0', SIZE + 1);
    memset(pipe_name, '\0', CLI_PIPE_SIZE + 1);    

    if (argc < 2) {
        //printf("Please specify the pathname of the server's pipe.\n");
        exit(EXIT_FAILURE);
    }

    server_pipe = argv[1];

    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe);

    //printf("[tfs_server] server_pipe : %s\n", server_pipe);

    if (unlink(server_pipe) != 0 && errno != ENOENT) {
        //printf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(server_pipe, PERMISSIONS) != 0) {
        //printf("[tfs_server] mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }


    //printf("[INFO - tfs_server] Server pipe = |%s|\n", server_pipe);

    
    if ((fserv = open(server_pipe, O_RDONLY)) < 0)  {
        //printf("[tfs_server] Open failed : %s\n", strerror(errno));
        return -1;
    }
        

    // ----------------------------------------- START RESPONDING TO REQUESTS --------------------------------------

    while (1) {

        memset(buffer, '\0', SIZE);

        n = read(fserv, buffer, SIZE_OF_CHAR);

        if (n == 0) { //EOF
            close(fserv);
            if ((fserv = open(server_pipe, O_RDONLY)) < 0) 
                return -1;
            continue;            
        }

        if (n == -1) {
            //printf("[ ERROR ] Reading : Failed\n");
            return -1;
        }

        command = ((int) buffer[0]) - '0';    

        switch(command) {
            case (TFS_OP_CODE_MOUNT):

                //printf("[INFO - tfs_server] Mounting...\n");

                if (tfs_handle_mount(pipe_name, fserv) == -1) {
                    //printf("[ tfs_server ] Error while mounting\n");
                    return -1;
                }

                if ((fcli = open(pipe_name, O_WRONLY)) < 0) 
                    return -1;

            break;

            case (TFS_OP_CODE_UNMOUNT):

                if (open_sessions == 0) {
                    //printf("[ tfs_server ] tfs_mount: There are no open sessions, please open one before unmount\n");
                    return -1;
                }
                open_sessions--;
            break;

            case (TFS_OP_CODE_OPEN):
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
                //printf("[tfs_server] Switch case : No correspondance\n");
                return -1;
            break;


        }
    }

    /* TO DO */

    return 0;
}