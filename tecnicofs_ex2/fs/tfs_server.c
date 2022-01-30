#include "operations.h"
#include "common/common.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#define SIZE 99

#define PERMISSIONS 0777
#define CLI_PIPE_SIZE 39
#define S 20
#define SIZE_OF_CHAR 1
#define NAME_PIPE_SIZE 40

typedef struct {
    int session_id;
    int fhandler;
    char name[40];
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
        printf("[ tfs_server ] All positions occupied\n");
        exit(EXIT_FAILURE);
    } 

    free_sessions[free] = TAKEN_POS;
    
    return free;
}

void tfs_handle_mount(char name[], int fserv) {

    int fcli, free = 0;
    ssize_t n = 0;
    char session_id_cli[sizeof(int)];

    memset(session_id_cli, '\0', sizeof(session_id_cli));
    memset(name, '\0', CLI_PIPE_SIZE + 1);

    // can't use slait because we don't know the exact size to read
    
    n = read(fserv, name, NAME_PIPE_SIZE);

    if (n == -1) {
        printf("[ ERROR ] Reading : Failed\n");
        exit(EXIT_FAILURE);
    }   

    if (open_sessions == S) {
        printf("[ tfs_server ] tfs_mount: Reached limit number of sessions, please wait\n");
        exit(EXIT_FAILURE);
    }

    if ((fcli = open(name, O_WRONLY)) < 0) {
        printf("[ tfs_server ] tfs_mount : Failed to open client pipe\n");
        exit(EXIT_FAILURE);
    }

    open_sessions++;

    free = find_free_pos();

    session_t *session = &(sessions[free]);

    memset(session->name, '\0', sizeof(session->name));

    session->session_id = free;
    session->fhandler = fcli;
    sprintf(session_id_cli, "%d", free);
    memcpy(session->name, name, strlen(name));

    n = write(fcli, session_id_cli, sizeof(int));

    if (n == -1) {
        printf("[ ERROR ] Reading : Failed\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_handle_read(int fserv) {
    char buffer[100];
    char aux_buffer[100];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    slait(buffer, sizeof(int), fserv);
    int session_id = atoi(buffer);

    slait(buffer, sizeof(int), fserv);
    int fhandle = atoi(buffer);

    slait(buffer, sizeof(int), fserv);
    size_t len = (size_t) atoi(buffer);

    char read_b[sizeof(char) * len + 1]; 
    memset(read_b, '\0', sizeof(read_b));

    //slait(buffer, len, fserv); //nao funciona aqui
    if (read(fserv, buffer, len) == -1) exit(EXIT_FAILURE);

    ssize_t read_bytes = tfs_read(fhandle, read_b, len);

    int fcli = sessions[session_id].fhandler;
    memset(buffer, '\0', sizeof(buffer));

    if (read_bytes < 0)  {
        
        sprintf(buffer, "%d", (int)read_bytes);

        ssize_t write_size = write(fcli, buffer, sizeof(int));

        if (write_size < 0) exit(EXIT_FAILURE);
        if(close(fcli) == -1) exit(EXIT_FAILURE);
        return;
    }

    char send[sizeof(int) + (sizeof(char) * len) + 1];
    memset(send, '\0', sizeof(send));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    sprintf(aux_buffer, "%d", (int)read_bytes);
    memcpy(send, aux_buffer, sizeof(int));
    memcpy(send + 4, read_b, (size_t)read_bytes);

    ssize_t write_size = write(fcli, send, sizeof(int) + (size_t)read_bytes);

    if (write_size < 0) exit(EXIT_FAILURE);
}

void tfs_handle_write(int fserv) {

    char buffer[SIZE];

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    int session_id = atoi(buffer);

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    int fhandle = atoi(buffer);

    slait(buffer, sizeof(int), fserv);
    size_t len = (size_t) atoi(buffer);

    memset(buffer, '\0', len);
    slait(buffer, len, fserv);

    ssize_t written = tfs_write(fhandle, buffer, len);

    if (written < 0 ) exit(EXIT_FAILURE);

    int fcli = sessions[session_id].fhandler;

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", (int)written);

    ssize_t write_size = write(fcli, buffer, sizeof(int));

    if (write_size < 0) exit(EXIT_FAILURE);
}

void tfs_handle_close(int fserv) {

    char buffer[SIZE];

    slait(buffer, sizeof(int), fserv);
    int session_id = atoi(buffer);

    slait(buffer, sizeof(int), fserv);
    int fhandle = atoi(buffer);

    int fclose = tfs_close(fhandle);

    if (fclose < 0) exit(EXIT_FAILURE);

    int fcli = sessions[session_id].fhandler;

    if (close(fcli) == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }
    fcli = open(sessions[session_id].name, O_WRONLY);

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", fclose);

    printf("[INFO - SERVER] fcli = |%s|\n", buffer);

    ssize_t write_size = write(fcli, buffer, sizeof(int));  

    printf("[ERROR - SERVER] (%ld) %s\n", write_size, strerror(errno));

    if (write_size < 0) exit(EXIT_FAILURE); 
}

void tfs_handle_unmount(int fserv) {

    char buffer[SIZE];
    char aux_buffer[SIZE];

    if (open_sessions == 0) {
        printf("[ tfs_server ] tfs_mount: There are no open sessions, please open one before unmount\n");
        exit(EXIT_FAILURE);
    }
    open_sessions--;

    slait(buffer, sizeof(int), fserv);

    // session_id
    memcpy(aux_buffer, buffer, sizeof(int));

    int session_id = atoi(aux_buffer);  

    if (close(fserv) == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 
    if (close(sessions[session_id].fhandler) == -1){
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 

    sessions[session_id].fhandler = -1;
    sessions[session_id].session_id = -1;
}

void tfs_handle_open(int fserv) {

    char name[CLI_PIPE_SIZE + 1];
    char buffer[SIZE];
    char aux_buffer[SIZE];

    memset(name, '\0', sizeof(name));
    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    ssize_t size_read = read(fserv, buffer, SIZE);
    if (size_read == -1) exit(EXIT_FAILURE);

    // session_id
    memcpy(aux_buffer, buffer, sizeof(int));

    int session_id = atoi(aux_buffer);  
    
    // name
    memcpy(name, buffer + 4, NAME_PIPE_SIZE);
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    // flags
    memcpy(aux_buffer, buffer + 44, sizeof(int));
    int flags = atoi(aux_buffer);

    int fhandler = tfs_open(name, flags);
    if (fhandler == -1) exit(EXIT_FAILURE);

    char fhandler_c = (char) (fhandler + '0');

    int fcli = sessions[session_id].fhandler;

    ssize_t write_size = write(fcli, &fhandler_c, sizeof(char));

    if (write_size < 0) exit(EXIT_FAILURE);

}

void tfs_handle_shutdown_after_all_close(int fserv) {

    char buffer[SIZE];
    char aux_buffer[SIZE];

    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    ssize_t size_read = read(fserv, buffer, SIZE);
    if (size_read == -1) exit(EXIT_FAILURE);

    // session_id
    memcpy(aux_buffer, buffer, sizeof(int));
    int session_id = atoi(aux_buffer);  

    int ret = tfs_destroy_after_all_closed();

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", ret);

    int fcli = sessions[session_id].fhandler;

    ssize_t write_size = write(fcli, buffer, sizeof(int));

    if (write_size == -1) exit(EXIT_FAILURE);

}

int main(int argc, char **argv) {

    /* TO DO */

    int fserv, command;
    ssize_t n;
    char *server_pipe;
    char buffer[SIZE];
    char name[CLI_PIPE_SIZE + 1];

    for (int i = 0; i < S; i++) {
        free_sessions[i] = FREE_POS;
    }

    open_sessions = 0;
    memset(buffer, '\0', SIZE);
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

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        printf("[ERROR]\n");
        return -1;
    }
        
    assert(tfs_init() != -1);

    if ((fserv = open(server_pipe, O_RDONLY)) < 0) {
        printf("[ ERROR ] Open server : %s\n", strerror(errno));
        return -1;
    }        

    // ----------------------------------------- START RESPONDING TO REQUESTS --------------------------------------

    while (1) {

        memset(buffer, '\0', SIZE);

        n = slait(buffer, sizeof(char), fserv);

        //printf("[INFO - SERVER] Buffer : %s\n", buffer);

        if (n == 0) { //EOF
            if (close(fserv) == -1) return -1;
            if ((fserv = open(server_pipe, O_RDONLY)) == -1) 
                return -1;
            continue;            
        } else if (n == -1 && errno == EBADF) {
            fserv = open(server_pipe, O_RDONLY);
            continue;
     /*   } else if (n == -1 && errno == ENOENT) {
            fserv = open(server_pipe, O_RDONLY);
            continue;
       */ } else if (n == -1) {
            printf("[ ERROR ] Reading : %s\n", strerror(errno));
            return -1;
        }

        printf("[INFO - SERVER] Open files atm = %d\n", open_sessions);

        command = atoi(buffer);

        switch(command) {
            case (TFS_OP_CODE_MOUNT):

                printf("[INFO - SERVER] Calling mount...\n");
                tfs_handle_mount(name, fserv);

            break;

            case (TFS_OP_CODE_UNMOUNT):

                printf("[INFO - SERVER] Calling unmount...\n");
                tfs_handle_unmount(fserv);

                // ir buscar a pipe do client com o session_id e responder através dela (assim sabe-se
                // que sessão terminar

            break;

            case (TFS_OP_CODE_OPEN):
                printf("[INFO - SERVER] Calling open...\n");

                tfs_handle_open(fserv);
                // ...

            break;

            case (TFS_OP_CODE_CLOSE):
                printf("[INFO - SERVER] Calling close...\n");
                tfs_handle_close(fserv);          

            break;

            case (TFS_OP_CODE_WRITE):

                printf("[INFO - SERVER] : Calling write...\n");
                tfs_handle_write(fserv);

            break;

            case (TFS_OP_CODE_READ):

                printf("[INFO - SERVER] : Calling read...\n");
                tfs_handle_read(fserv);              

            break;

            case (TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED):

                printf("[INFO - SERVER] : Calling shutdown...\n");
                tfs_handle_shutdown_after_all_close(fserv);

            break;

            default:
                printf("[tfs_server] Switch case : Command %c: No correspondance\n", command);
            break;


        }
    }

    

    return 0;
}