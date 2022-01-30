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

// Specifies the max number of sessions existing simultaneously
#define S 20

#define SIZE 99
#define PERMISSIONS 0777

#define CLI_PIPE_SIZE 39
#define SIZE_OF_CHAR sizeof(char)
#define SIZE_OF_INT sizeof(int)
#define NAME_SIZE 40

typedef struct {
    int session_id;
    char name[NAME_SIZE];
    int fhandler;    
} session_t;

typedef enum {FREE_POS = 1, TAKEN_POS = 0} session_state_t;

static int open_sessions;
static session_t sessions[S];
static session_state_t free_sessions[S];

int find_free_pos() {

    // This would be the number of the session that will be created, if possible
    int session_free = -1; 
    
    for (int i = 0; i < S; i++) {
        if (free_sessions[i] == FREE_POS) {            
            session_free = i;
            break;
        }
    }

    // No sessions available, all positions are marked as "TAKEN"
    if (session_free == -1) {
        printf("[ERROR - SERVER] Server is full, please try again\n");
        return session_free;
    } 

    free_sessions[session_free] = TAKEN_POS;
    
    return session_free;
}

void tfs_handle_mount(char name[], int fserv) {

    int fcli, free_session_id = 0;
    ssize_t ret = 0;
    char session_id_cli[sizeof(int)];

    memset(session_id_cli, '\0', sizeof(session_id_cli));
    memset(name, '\0', CLI_PIPE_SIZE + 1);

    // READ CLIENT'S PIPE NAME    
    ret = slait(name, NAME_SIZE, fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] Reading : Failed : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }   

    // OPEN CLIENT'S PIPE
    if ((fcli = open(name, O_WRONLY)) < 0) {
        printf("[ERROR - SERVER] Failed : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (open_sessions == S) {
        printf("[ERROR - SERVER] Reached limit number of sessions, please wait\n");
        
        // INFORM CLIENT THAT THE SESSION CAN'T BE CREATED
        sprintf(session_id_cli, "%d", -1);
        if (write(fcli, session_id_cli, sizeof(int)) == -1) {
            printf("[ERROR - SERVER] Writing to client : %s\n", strerror(errno));
        }
        if (close(fcli) == -1) {
            printf("[ERROR - SERVER] Closing pipe : %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        return;
    }    

    free_session_id = find_free_pos();

    // ERROR ASSIGNING SESSION ID
    if (free_session_id == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        sprintf(session_id_cli, "%d", free_session_id);
        if (write(fcli, session_id_cli, sizeof(int)) == -1) {
            printf("[ERROR - SERVER] Writing to client : %s\n", strerror(errno));
        }
        return;
    }

    session_t *session = &(sessions[free_session_id]);

    memset(session->name, '\0', sizeof(session->name));

    session->session_id = free_session_id;
    session->fhandler = fcli;    
    memcpy(session->name, name, strlen(name));

    // SERVER RESPONSE TO CLIENT

    sprintf(session_id_cli, "%d", free_session_id);
    ret = write(fcli, session_id_cli, sizeof(int));

    if (ret == -1) {
        printf("[ERROR - SERVER] Writing : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    open_sessions++;
}

void tfs_handle_unmount(int fserv) {

    char buffer[SIZE];
    char aux[SIZE];
    memset(buffer, '\0', SIZE);
    memset(aux, '\0', SIZE);

    // READ SERVER MESSAGE
    ssize_t ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    // SESSION ID
    memcpy(aux, buffer, sizeof(int));
    int session_id = atoi(aux);  

    if (open_sessions == 0 || session_id == -1) {
        printf("[ERROR - SERVER] There are no open sessions, please open one before unmount\n");
        // como dar handle nesta situação?
        return;
    }

    // CLOSE & ERASE CLIENT
    if (close(sessions[session_id].fhandler) == -1){
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    sessions[session_id].fhandler = -1;
    sessions[session_id].session_id = -1;
    memset(sessions[session_id].name, '\0', NAME_SIZE);

    open_sessions--;

}

void tfs_handle_read(int fserv) {
    char buffer[100];
    char aux_buffer[100];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    ssize_t ret;

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    int session_id = atoi(buffer);

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    int fhandle = atoi(buffer);

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

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
    ssize_t ret;

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    int session_id = atoi(buffer);

    if (read(fserv, buffer, sizeof(int)) == -1) exit(EXIT_FAILURE);
    int fhandle = atoi(buffer);

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    size_t len = (size_t) atoi(buffer);

    memset(buffer, '\0', len);
    ret = slait(buffer, len, fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

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
    ssize_t ret;

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    int session_id = atoi(buffer);

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }
    
    int fhandle = atoi(buffer);

    int fclose = tfs_close(fhandle);
    if (fclose < 0) {
        printf("[ERROR - SERVER] Error closing\n");
        exit(EXIT_FAILURE);
    } 

    int fcli = sessions[session_id].fhandler;

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", fclose);

    ssize_t write_size = write(fcli, buffer, sizeof(int));  
    if (write_size < 0) exit(EXIT_FAILURE); 
}

void tfs_handle_open(int fserv) {

    char name[CLI_PIPE_SIZE + 1];
    char buffer[SIZE];
    char aux[SIZE];

    size_t max_size_for_open_message = 45; // int ssid, char[40] name, char flags
    size_t len = 0;

    memset(name, '\0', sizeof(name));
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    ssize_t size_read = slait(buffer, max_size_for_open_message, fserv);
    if (size_read == -1) {
        printf("[ERROR - SERVER] Reading : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 

    // SESSION_ID
    memcpy(aux, buffer, sizeof(int));
    len += sizeof(int);
    int session_id = atoi(aux);      
    
    // NAME
    memcpy(name, buffer + len, NAME_SIZE);
    len += NAME_SIZE;
    memset(aux, '\0', sizeof(aux));

    // FLAGS
    memcpy(aux, buffer + len, sizeof(int));
    len += sizeof(int);
    int flags = atoi(aux);

    // OPEN FILE IN TFS
    int tfs_fhandler = tfs_open(name, flags);
    if (tfs_fhandler == -1) {
        printf("[ERROR - SERVER] Open tfs\n");
        exit(EXIT_FAILURE);
    } 

    int fcli = sessions[session_id].fhandler; //this is client api fhandler

    memset(aux, '\0', SIZE);
    sprintf(aux, "%d", tfs_fhandler);

    ssize_t write_size = write(fcli, aux, sizeof(int));
    if (write_size < 0) {
        printf("[ERROR - SERVER] Error writing : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 

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

    int fserv, command;
    ssize_t n;
    char *server_pipe;
    char buffer[SIZE];
    char name[CLI_PIPE_SIZE + 1];

    open_sessions = 0;
    memset(buffer, '\0', SIZE);
    memset(name, '\0', CLI_PIPE_SIZE + 1); 

    for (int i = 0; i < S; i++) {
        free_sessions[i] = FREE_POS;
        sessions[i].fhandler = -1;
        sessions[i].session_id = -1;
        memset(sessions[i].name, '\0', NAME_SIZE);
    }   

    if (argc < 2) {
        printf("[INFO - SERVER] Please specify the pathname of the server's pipe.\n");
        exit(EXIT_FAILURE);
    }

    // CRIACAO DO SERVIDOR 

    server_pipe = argv[1];

    printf("[INFO - SERVER] Starting TecnicoFS server with pipe called %s\n", server_pipe);

    if (unlink(server_pipe) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERROR - SERVER]: unlink(%s) failed: %s\n", server_pipe, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(server_pipe, PERMISSIONS) != 0) {
        printf("[ERROR - SERVER] mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
        
    if (tfs_init() == -1) {
        printf("[ERROR - SERVER] TFS init failed\n");
        exit(EXIT_FAILURE);
    }

    if ((fserv = open(server_pipe, O_RDONLY)) < 0) {
        printf("[ERROR - SERVER] Open server : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }        

    // ----------------------------------------- START RESPONDING TO REQUESTS --------------------------------------

    while (1) {

        memset(buffer, '\0', SIZE);

        n = slait(buffer, sizeof(char), fserv);

        if (n == 0) { //EOF
            if (close(fserv) == -1) return -1;
            if ((fserv = open(server_pipe, O_RDONLY)) == -1) 
                return -1;
            continue;            
        } 
        else if (n == -1 && errno == EBADF) {
            fserv = open(server_pipe, O_RDONLY);
            continue;
        } 
        else if (n == -1) {
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