#include "operations.h"
#include "client/tecnicofs_client_api.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PERMISSIONS 0777
#define SIZE 100
#define CLI_SIZE 40

#define S 20
#define SIZE_OF_CHAR 1
#define NAME_PIPE_SIZE 40

static int open_sessions;

typedef struct {
    int session_id;
    char pipe_name[40];
    // client ??
} session_t;

int main(int argc, char **argv) {

    session_t sessions[S];
    int free_sessions[S];
    int fserv;
    ssize_t n;
    char command;
    char *server_pipe;

    char buffer[SIZE];

    memset(free_sessions, 1, sizeof(free_sessions));

    open_sessions = 0;

    // ---------------------------------------- OPEN SERVER PIPE ---------------------------------------------

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }
    else {
        server_pipe = argv[1];
    }

    unlink(server_pipe);

    if (mkfifo(server_pipe, PERMISSIONS) < 0) {
        return -1;
    }

    if ((fserv = open(server_pipe, O_RDONLY)) < 0) 
        return -1;

    // ----------------------------------------- START RESPONDING TO REQUESTS --------------------------------------

    while (1) {

        memset(buffer, '\0', SIZE);

        n = read(fserv, buffer, SIZE_OF_CHAR);

        if (n == 0) {
            close(fserv);
            if ((fserv = open(server_pipe, O_RDONLY)) < 0) 
                return -1;
            continue;            
        }

        if (n == -1) {
            printf("[ ERROR ] Reading : Failed\n");
            return -1;
        }

        command = buffer[0];      

        switch(command) {
            case (TFS_OP_CODE_MOUNT):

                memset(buffer, '\0', sizeof(buffer));
                n = read(fserv, buffer, NAME_PIPE_SIZE);

                if (n == -1) {
                    printf("[ ERROR ] Reading : Failed\n");
                    return -1;
                }

                buffer[n] = '\0';

                if (open_sessions == S) {
                    printf("[ tfs_server ] tfs_mount: Reached limit number of sessions, please wait\n");
                    return -1;
                }
                open_sessions++;

                // choose place for the new session
                // choose the array position to put the new session

                int free = -1;

                for (int i = 0; i < S; i++) {
                    if (free_sessions[i] == 1) {
                        free = i;
                        break;
                    }
                }

                if (free == -1) return -1;

                free_sessions[free] = 0;

                session_t *session = &(sessions[free]);

                size_t len = strlen(buffer);

                memset(session->pipe_name, '\0', sizeof(session->pipe_name));

                memcpy(session->pipe_name, buffer, strlen(buffer)));
                session->pipe_name[len] = '\0';

                // choose session id
                
                session->session_id = free;

            break;

            case (TFS_OP_CODE_UNMOUNT):

                if (open_sessions == 0) {
                    printf("[ tfs_server ] tfs_mount: There are no open sessions, please open one before unmount\n");
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
            break;


        }
    }



    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */

    return 0;
}