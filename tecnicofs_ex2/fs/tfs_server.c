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
#include <pthread.h>

// Specifies the max number of sessions existing simultaneously
#define S 2
#define SIZE 100
#define PERMISSIONS 0777
#define SIZE_OF_CHAR sizeof(char)
#define SIZE_OF_INT sizeof(int)
#define NAME_SIZE 40

typedef struct {
    int session_id;
    char name[NAME_SIZE];
    int fhandler;    
} session_t;

typedef struct {
    int fhandler;
    size_t len;
    int flags;
    char name[40];
    // ...
} request_t;

typedef struct {
    pthread_t tid;
    int session_id;
    int wake_up;
    pthread_cond_t work;
    request_t request;
} slave_t;

typedef enum {FREE_POS = 1, TAKEN_POS = 0} session_state_t;

static int open_sessions;
static session_t sessions[S];
static session_state_t free_sessions[S];
slave_t slaves[S];

void handle_error() {

}

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
    memset(name, '\0', NAME_SIZE);

    // READ CLIENT'S PIPE NAME FROM SERVER PIPE 
    ret = slait(name, NAME_SIZE, fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] Reading : Failed : %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : 
        exit(EXIT_FAILURE);
    }   

    // OPEN CLIENT'S PIPE
    if ((fcli = open(name, O_WRONLY)) < 0) {
        printf("[ERROR - SERVER] Failed : %s\n", strerror(errno));
        // ERROR : OPEN CLIENT PIPE
        // CAUSES : ENOENT, EINTR
        // HANDLE : ENOENT -> keep trying until open is successfull
        //          EINTR -> retry until success (TEMP_FAILURE_RETRY like)
        exit(EXIT_FAILURE);
    }

    if (open_sessions == S) {
        printf("[ERROR - SERVER] Reached limit number of sessions, please wait\n");
        
        // INFORM CLIENT THAT THE SESSION CAN'T BE CREATED
        sprintf(session_id_cli, "%d", -1);
        if (write(fcli, session_id_cli, sizeof(int)) == -1) {
            printf("[ERROR - SERVER] Writing to client : %s\n", strerror(errno));
            // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
        }
        if (close(fcli) == -1) {
            printf("[ERROR - SERVER] Closing pipe : %s\n", strerror(errno));
            // ERROR : CLOSE CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> CLOSE AND OPEN CLIENT (?)
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
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
            // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
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
        // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
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
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
    }

    // SESSION ID
    memcpy(aux, buffer, sizeof(int));
    int session_id = atoi(aux);  

    if (open_sessions == 0 || session_id == -1) {
        printf("[ERROR - SERVER] There are no open sessions, please open one before unmount\n");
        // ERROR : NO OPEN SESSIONS AKA INVALID SESSION
        // CAUSES : NO CLIENTS HAVE DONE A SUCCESSFULL MOUNT 
        // HANDLE : ignore client
        return;
    }

    // CLOSE & ERASE CLIENT
    if (close(sessions[session_id].fhandler) == -1){
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : CLOSE CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> CLOSE AND OPEN CLIENT (?)
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
        exit(EXIT_FAILURE);
    }

    sessions[session_id].fhandler = -1;
    sessions[session_id].session_id = -1;
    memset(sessions[session_id].name, '\0', NAME_SIZE);

    open_sessions--;
    free_sessions[session_id] = FREE_POS;

    printf("[INFO - SERVER] Open sessions on unmount = %d\n", open_sessions);

}

void tfs_handle_read(int fserv) {
    char buffer[SIZE];
    char aux[SIZE];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    ssize_t ret = 0;

    // SESSION ID
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
    }
    int session_id = atoi(buffer);

    // F HANDLE
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
    }
    int fhandle = atoi(buffer);

    // LEN
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
    }
    size_t len = (size_t) atoi(buffer);

    char ouput_tfs[sizeof(char) * len]; 
    memset(ouput_tfs, '\0', sizeof(ouput_tfs));

    ssize_t read_bytes = tfs_read(fhandle, ouput_tfs, len);

    int fcli = sessions[session_id].fhandler;
    memset(buffer, '\0', sizeof(buffer));

    if (read_bytes < 0)  {
        
        sprintf(buffer, "%d", (int)read_bytes);
        ssize_t write_size = write(fcli, buffer, sizeof(int));

        if (write_size < 0){
            printf("[ERROR - SERVER] Error writing: %s\n", strerror(errno));
            // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
        }
        return;
    }

    char send[sizeof(int) + (sizeof(char) * len)];
    memset(send, '\0', sizeof(send));
    memset(aux, '\0', sizeof(aux));

    sprintf(aux, "%d", (int)read_bytes);
    memcpy(send, aux, sizeof(int));

    memcpy(send + sizeof(int), ouput_tfs, (size_t)read_bytes);

    ssize_t write_size = write(fcli, send, sizeof(int) + (size_t)read_bytes);

    if (write_size < 0) {
        printf("[ERROR - SERVER] Error writing : %s\n", strerror(errno));
        // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
    }
}

void tfs_handle_write(int fserv) {

    char buffer[SIZE];
    ssize_t ret;

    // SESSION_ID
    if (slait(buffer, sizeof(int), fserv) == -1) {
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
        exit(EXIT_FAILURE);
    } 
    int session_id = atoi(buffer);

    // FHANDLE
    if (slait(buffer, sizeof(int), fserv) == -1){
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
        exit(EXIT_FAILURE);
    }
    int fhandle = atoi(buffer);

    // LEN
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
        exit(EXIT_FAILURE);
    }
    size_t len = (size_t) atoi(buffer);

    // TEXT TO WRITE ON TFS
    memset(buffer, '\0', len);
    ret = slait(buffer, len, fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
        exit(EXIT_FAILURE);
    }

    ssize_t written = tfs_write(fhandle, buffer, len);

    if (written < 0 ) {
        // ERROR : WRITING ON FILE SYSTEM
        // CAUSES : INTERNAL ERROR
        // HANDLE : responde to client, move on
        exit(EXIT_FAILURE);
    } 

    int fcli = sessions[session_id].fhandler;

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", (int)written);

    ssize_t write_size = write(fcli, buffer, sizeof(int));
    if (write_size < 0) {
        // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
        exit(EXIT_FAILURE);
    }
}

void tfs_handle_close(int fserv) {

    char buffer[SIZE];
    ssize_t ret;

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
        exit(EXIT_FAILURE);
    }

    int session_id = atoi(buffer);

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
        exit(EXIT_FAILURE);
    }
    
    int fhandle = atoi(buffer);

    int fclose = tfs_close(fhandle);
    if (fclose < 0) {
        printf("[ERROR - SERVER] Error closing\n");
        // ERROR : CLOSING FILE SYSTEM
        // CAUSES : INTERNAL ERROR
        // HANDLE : responde to client, move on
        exit(EXIT_FAILURE);
    } 

    int fcli = sessions[session_id].fhandler;

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", fclose);

    ssize_t write_size = write(fcli, buffer, sizeof(int));  
    if (write_size < 0) {
        // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
    } 
}

void tfs_handle_open(int fserv) {

    char name[NAME_SIZE];
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
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
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
        // ERROR : OPEN FILE SYSTEM
        // CAUSES : INTERNAL ERROR
        // HANDLE : responde to client, move on
        exit(EXIT_FAILURE);
    } 

    int fcli = sessions[session_id].fhandler; //this is client api fhandler

    memset(aux, '\0', SIZE);
    sprintf(aux, "%d", tfs_fhandler);

    ssize_t write_size = write(fcli, aux, sizeof(int));
    if (write_size < 0) {
        printf("[ERROR - SERVER] Error writing : %s\n", strerror(errno));
        // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
        exit(EXIT_FAILURE);
    } 

}

void tfs_handle_shutdown_after_all_close(int fserv) {

    char buffer[SIZE];
    char aux_buffer[SIZE];

    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    ssize_t size_read = slait(buffer, SIZE, fserv);
    if (size_read == -1) {
        // ERROR : READING CLIENT REQUEST
        // CAUSES : EBADF, EINTR, ENOENT
        // HANDLE : EBADF -> SERVER ERROR, ????
        //          EINTR -> try again
        //          ENOENT -> same as EBADF ???
    }

    // session_id
    memcpy(aux_buffer, buffer, sizeof(int));
    int session_id = atoi(aux_buffer);  

    int ret = tfs_destroy_after_all_closed();

    if (ret == -1) {
        printf("[ERROR - SERVER] Destroy failed\n");
        // ERROR : OPEN FILE SYSTEM
        // CAUSES : INTERNAL ERROR
        // HANDLE : responde to client, move on
    }

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", ret);

    int fcli = sessions[session_id].fhandler;

    ssize_t write_size = write(fcli, buffer, sizeof(int));

    if (write_size == -1) {
        // ERROR : WRITE ON CLIENT PIPE
            // CAUSES : EPIPE, EBADF, ENOENT, EINTR
            // HANDLE : EPIPE -> erase the client, declare him as dead
            //          EBADF -> same handle as EPIPE?
            //          ENOENT -> CLOSE AND OPEN CLIENT (?)
            //          EINTR -> TEMP_FAILURE_RETRY like
    }

}

int server_init(char *buffer, char *name) {
    open_sessions = 0;
    memset(buffer, '\0', SIZE);
    memset(name, '\0', NAME_SIZE);

    for (int i = 0; i < S; i++) {
        free_sessions[i] = FREE_POS;
        sessions[i].fhandler = -1;
        sessions[i].session_id = -1;
        memset(sessions[i].name, '\0', NAME_SIZE);
    }

    for (int i = 0; i < S; i++) {
        pthread_init();
    }



}

int main(int argc, char **argv) {

    int fserv, command;
    ssize_t n;
    char *server_pipe;
    char buffer[SIZE];
    char name[NAME_SIZE];  

    if (argc < 2) {
        printf("[INFO - SERVER] Please specify the pathname of the server's pipe.\n");
        exit(EXIT_FAILURE);
    }

    // CRIACAO DO SERVIDOR 

    if (server_init(buffer, name) == -1) {
        printf("[ERROR - SERVER] Error starting server\n");
    }

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

        command = atoi(buffer);

        switch(command) {
            case (TFS_OP_CODE_MOUNT):

                printf("[INFO - SERVER] Calling mount...\n");
                tfs_handle_mount(name, fserv);

            break;

            case (TFS_OP_CODE_UNMOUNT):

                printf("[INFO - SERVER] Calling unmount...\n");
                tfs_handle_unmount(fserv);

            break;

            case (TFS_OP_CODE_OPEN):
                printf("[INFO - SERVER] Calling open...\n");

                tfs_handle_open(fserv);

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