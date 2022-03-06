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

#define S 10
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
    int op_code;
    int fcli;
    int fhandler;
    size_t len;
    int flags;
    char name[NAME_SIZE];
    char *to_write;
} request_t;

typedef struct {
    pthread_t tid;
    int session_id;
    int wake_up;
    pthread_cond_t work_cond;
    request_t request;
    pthread_mutex_t slave_mutex;
    pthread_rwlock_t slave_rdlock;
} slave_t;

typedef enum {FREE_POS = 1, TAKEN_POS = 0} session_state_t;

static pthread_t counter_thread;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static int counter_operations;

static int open_sessions;
static int fserv;
static session_t sessions[S];
static session_state_t free_sessions[S];
slave_t slaves[S];

pthread_rwlock_t read_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

void increment_counter() {
    if (pthread_mutex_lock(&counter_mutex) != 0) exit(EXIT_FAILURE);
    counter_operations++;
    if (pthread_mutex_unlock(&counter_mutex) != 0) exit(EXIT_FAILURE);
}

void counter_ops() {

    while(1) {
        sleep(2);
        printf("[INFO - SERVER] Until now, a total of %d operations were performed\n", counter_operations);
    }
    
}

void immortal() {
    signal(SIGINT, SIG_IGN);
    printf("[INFO - SERVER] Do you really wanna kill the server? y or n\n\n===> PS: From this point, it's immortal <===\n");
    char ch = '\0';
    scanf("%c", &ch);
    if (ch == 'y')
        exit(EXIT_SUCCESS);
}

void erase_client(int session_id) {

    sessions[session_id].fhandler = -1;
    sessions[session_id].session_id = -1;
    memset(sessions[session_id].name, '\0', NAME_SIZE);            

    if (pthread_mutex_lock(&global_mutex) == -1) {
        printf("[ERROR - SERVER] Failed locking mutex\n");
    }
    open_sessions--;
    free_sessions[session_id] = FREE_POS;
    if (pthread_mutex_unlock(&global_mutex) == -1) {
        printf("[ERROR - SERVER] Failed unlocking mutex\n");
    }

}

int slait_open(char *name) {
    int fcli;
    while(1) {
        fcli = open(name, O_WRONLY | O_NONBLOCK);
        if (fcli != -1 ) break;
        printf("[ERROR - SERVER] Failed on client %s : %s\n", name, strerror(errno));
        printf("[ERROR - SERVER] Trying to open client %s again\n", name);
    }
    printf("[INFO - SERVER] Sucess opening client %s with fcli %d\n", name, fcli);
    return fcli;
}

ssize_t slait_write(int fcli, char *buffer, size_t len) {

    ssize_t written_count = 0, written = 0;

    while(1) {
        written = write(fcli, buffer, len);  
        if (written == -1) {
            printf("[ERROR - SERVER] Error reading file, %s\n", strerror(errno));
            return written;
        }
        else if (written == 0) {
            printf("[INFO - SERVER] EOF\n");
            return written;
        }
        written_count += written;
        if (written_count >= len)
            break;
    }
    return written;

}

ssize_t slait(char *buffer_c, size_t len, int cli_fh) {

    ssize_t written_count = 0, written_tfs = 0;

    while(1) {
        written_tfs = read(cli_fh, buffer_c + written_count, len);  
        if (written_tfs == -1) {
            printf("[ERROR - SERVER] Error reading file, %s\n", strerror(errno));
            return written_tfs;
        }

        else if (written_tfs == 0) {
            printf("[INFO - SERVER] EOF\n");
            return written_tfs;
        }

        written_count += written_tfs;

        if (written_count >= len)
            break;
    }
    return written_tfs;
}

int handle_error(int error, int session_id) {

    switch(error) {
        case(EBADF):
            // erase client
            erase_client(session_id);            
            return -1;
            break;  

        case (EPIPE):
            // erase client
            erase_client(session_id);            
            return 0;
            break; 

        case(ENOENT):
            // try again
            return slait_open(sessions[session_id].name);
            break;
        
        case(EINTR):
            // try again
            return slait_open(sessions[session_id].name);
            break;
        default: 
            return -1;
            break;      
    }
    return 0;
}

int find_free_pos() {

    // This would be the number of the session that will be created, if possible
    int session_free = -1; 

    pthread_mutex_lock(&global_mutex);
    
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

    pthread_mutex_unlock(&global_mutex);
    
    return session_free;
}

void tfs_handle_mount(char name[]) {

    int fcli, free_session_id = 0;
    ssize_t ret = 0;
    char session_id_cli[sizeof(int)];

    memset(session_id_cli, '\0', sizeof(session_id_cli));
    memset(name, '\0', NAME_SIZE);

    // READ CLIENT'S PIPE NAME FROM SERVER PIPE
    ret = slait(name, NAME_SIZE, fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] Reading : Failed : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }  

    fcli = slait_open(name);

    if (open_sessions == S) {
        printf("[ERROR - SERVER] Reached limit number of sessions, please wait\n");
        
        // INFORM CLIENT THAT THE SESSION CAN'T BE CREATED
        sprintf(session_id_cli, "%d", -1);
        slait_write(fcli, session_id_cli, sizeof(int));

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
        slait_write(fcli, session_id_cli, sizeof(int));
        return;
    }

    // LEAVE IT TO SLAVES

    pthread_mutex_lock(&slaves[free_session_id].slave_mutex);

    slaves[free_session_id].request.op_code = TFS_OP_CODE_MOUNT;
    slaves[free_session_id].request.fcli = fcli;
    slaves[free_session_id].wake_up = 1;
    pthread_cond_signal(&slaves[free_session_id].work_cond);

    pthread_mutex_unlock(&slaves[free_session_id].slave_mutex);

    pthread_mutex_lock(&global_mutex);
    open_sessions++;
    pthread_mutex_unlock(&global_mutex);
}

void tfs_handle_unmount() {

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
        return;
    }

    // REQUEST PARSED

    pthread_mutex_lock(&slaves[session_id].slave_mutex);

    slaves[session_id].request.op_code = TFS_OP_CODE_UNMOUNT;    
    slaves[session_id].wake_up = 1;
    pthread_cond_signal(&slaves[session_id].work_cond);

    pthread_mutex_unlock(&slaves[session_id].slave_mutex);

    pthread_mutex_lock(&global_mutex);
    open_sessions--;
    pthread_mutex_unlock(&global_mutex);
}

void tfs_handle_read() {
    char buffer[SIZE];
    char aux[SIZE];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    ssize_t ret = 0;

    // SESSION ID
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    int session_id = atoi(buffer);

    // F HANDLE
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }
    
    int fhandle = atoi(buffer);

    pthread_rwlock_rdlock(&read_lock);
    int fcli = sessions[session_id].fhandler; // this fhandler belongs to the client itself
    pthread_rwlock_unlock(&read_lock);

    if (fhandle == -1) {
        slait_write(fcli, buffer, sizeof(int));
        return;
    }

    // LEN
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
    }

    size_t len = (size_t) atoi(buffer);

    // LEAVE IT TO THREADS
    pthread_mutex_lock(&slaves[session_id].slave_mutex);

    slaves[session_id].request.op_code = TFS_OP_CODE_READ;
    slaves[session_id].request.fhandler = fhandle;
    slaves[session_id].request.len = len;
    slaves[session_id].wake_up = 1;

    pthread_cond_signal(&slaves[session_id].work_cond);

    pthread_mutex_unlock(&slaves[session_id].slave_mutex);

    increment_counter();
}

void tfs_handle_write() {

    char buffer[SIZE];
    ssize_t ret;

    // SESSION_ID
    if (slait(buffer, sizeof(int), fserv) == -1) {
        exit(EXIT_FAILURE);
    } 

    int session_id = atoi(buffer);

    // FHANDLE
    if (slait(buffer, sizeof(int), fserv) == -1){
        exit(EXIT_FAILURE);
    }

    int fhandle = atoi(buffer);

    pthread_rwlock_rdlock(&read_lock);
    int fcli = sessions[session_id].fhandler; // this fhandler belongs to the client itself
    pthread_rwlock_unlock(&read_lock);

    if (fhandle == -1) {
        slait_write(fcli, buffer, sizeof(int));
        return;
    }

    // LEN
    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    size_t len = (size_t) atoi(buffer);

    // TEXT TO WRITE ON TFS
    memset(buffer, '\0', len);
    ret = slait(buffer, len, fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // LEAVE IT TO SLAVE

    pthread_mutex_lock(&slaves[session_id].slave_mutex);

    slaves[session_id].request.op_code = TFS_OP_CODE_WRITE;
    slaves[session_id].request.fhandler = fhandle;
    slaves[session_id].request.len = len;
    slaves[session_id].request.to_write = (char *)malloc(sizeof(char) * len);
    memcpy(slaves[session_id].request.to_write, buffer, len);    
    slaves[session_id].wake_up = 1;

    pthread_cond_signal(&slaves[session_id].work_cond);

    pthread_mutex_unlock(&slaves[session_id].slave_mutex);

    increment_counter();
}

void tfs_handle_close() {

    char buffer[SIZE];
    ssize_t ret;

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int session_id = atoi(buffer);

    ret = slait(buffer, sizeof(int), fserv);
    if (ret == -1) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    int fhandle = atoi(buffer);

    pthread_rwlock_rdlock(&read_lock);
    int fcli = sessions[session_id].fhandler; // this fhandler belongs to the client itself
    pthread_rwlock_unlock(&read_lock);

    if (fhandle == -1) {
        slait_write(fcli, buffer, sizeof(int));
        return;
    }

    // REQUEST PARSED

    pthread_mutex_lock(&slaves[session_id].slave_mutex);

    slaves[session_id].request.op_code = TFS_OP_CODE_CLOSE;    
    slaves[session_id].request.fhandler = fhandle;
    slaves[session_id].wake_up = 1;

    pthread_cond_signal(&slaves[session_id].work_cond);

    pthread_mutex_unlock(&slaves[session_id].slave_mutex);

    increment_counter();
}

void tfs_handle_open() {

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

    // REQUEST PARSED

    pthread_mutex_lock(&slaves[session_id].slave_mutex);

    slaves[session_id].request.op_code = TFS_OP_CODE_OPEN;
    memcpy(slaves[session_id].request.name, name, NAME_SIZE);
    slaves[session_id].request.flags = flags;
    slaves[session_id].wake_up = 1;
    pthread_cond_signal(&slaves[session_id].work_cond);

    pthread_mutex_unlock(&slaves[session_id].slave_mutex);

    increment_counter();
}

int tfs_handle_shutdown_after_all_close() {  

    char buffer[SIZE];
    char aux_buffer[SIZE];

    memset(buffer, '\0', sizeof(buffer));
    memset(aux_buffer, '\0', sizeof(aux_buffer));

    ssize_t size_read = slait(buffer, sizeof(int), fserv);
    if (size_read == -1) {
    }

    // session_id
    memcpy(aux_buffer, buffer, sizeof(int));
    int session_id = atoi(aux_buffer);  

    pthread_mutex_lock(&slaves[session_id].slave_mutex);

    slaves[session_id].request.op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;    
    slaves[session_id].wake_up = 1;
    pthread_cond_signal(&slaves[session_id].work_cond);

    pthread_mutex_unlock(&slaves[session_id].slave_mutex);

    return 0;
}

void tfs_thread_mount(slave_t *slave) {

    char session_id_cli[SIZE];
    memset(session_id_cli, '\0', SIZE);

    pthread_mutex_lock(&global_mutex);
    session_t *session = &(sessions[slave->session_id]);
    pthread_mutex_unlock(&global_mutex);

    memset(session->name, '\0', sizeof(session->name));

    session->session_id = slave->session_id;
    session->fhandler = slave->request.fcli;    
    memcpy(session->name, slave->request.name, strlen(slave->request.name));

    // SERVER RESPONSE TO CLIENT
    sprintf(session_id_cli, "%d", slave->session_id);
    int fcli = slave->request.fcli;

    slait_write(fcli, session_id_cli, sizeof(int));
}

void tfs_thread_unmount(slave_t *slave) {

    // CLOSE & ERASE CLIENT
    if (close(sessions[slave->session_id].fhandler) == -1){
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int session_id = slave->session_id;

    sessions[session_id].fhandler = -1;
    sessions[session_id].session_id = -1;
    memset(sessions[session_id].name, '\0', NAME_SIZE);

    pthread_mutex_lock(&global_mutex);
    free_sessions[session_id] = FREE_POS;
    pthread_mutex_unlock(&global_mutex);
}

void tfs_thread_open(slave_t *slave) {

    char aux[SIZE];
    memset(aux, '\0', SIZE);

    memcpy(aux, slave->request.name, sizeof(slave->request.name));
    int flags = slave->request.flags;
    int session_id = slave->session_id;

    // OPEN FILE IN TFS
    int tfs_fhandler = tfs_open(aux, flags);

    memset(aux, '\0', SIZE);
    sprintf(aux, "%d", tfs_fhandler);

    int fcli = sessions[session_id].fhandler; // this fhandler belongs to the client itself

    slait_write(fcli, aux, sizeof(int));

}

void tfs_thread_close(slave_t *slave) {

    char buffer[SIZE];

    int fhandler = slave->request.fhandler;
    int session_id = slave->session_id;

    int fclose = tfs_close(fhandler);
    if (fclose < 0) {
        printf("[ERROR - SERVER] Error closing\n");
    } 

    pthread_rwlock_rdlock(&read_lock);
    int fcli = sessions[session_id].fhandler;
    pthread_rwlock_unlock(&read_lock);


    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", fclose);

    ssize_t write_size = write(fcli, buffer, sizeof(int));  
    if (write_size < 0) {
        // ERROR : WRITE ON CLIENT PIPE
        printf("[ERROR - SERVER] Error writing: %s\n", strerror(errno));
    } 
}

void tfs_thread_read(slave_t *slave) {

    char buffer[SIZE];
    char aux[SIZE];

    int session_id = slave->session_id;
    size_t len = slave->request.len;
    int fhandler = slave->request.fhandler;

    pthread_rwlock_rdlock(&read_lock);
    int fcli = sessions[session_id].fhandler; // this fhandler belongs to the client itself
    pthread_rwlock_unlock(&read_lock);

    if (fhandler == -1) {
        slait_write(fcli, buffer, sizeof(int));
        return;
    }

    char ouput_tfs[sizeof(char) * len]; 
    memset(ouput_tfs, '\0', sizeof(ouput_tfs));

    ssize_t read_bytes = tfs_read(fhandler, ouput_tfs, len);

    memset(buffer, '\0', sizeof(buffer));

    if (read_bytes < 0)  {        
        sprintf(buffer, "%d", (int)read_bytes);
        ssize_t write_size = write(fcli, buffer, sizeof(int));

        if (write_size < 0){
            printf("[ERROR - SERVER] Error writing: %s\n", strerror(errno));
        }
        return;
    }

    char send[sizeof(int) + (sizeof(char) * len)];
    memset(send, '\0', sizeof(send));
    memset(aux, '\0', sizeof(aux));

    sprintf(aux, "%d", (int)read_bytes);
    memcpy(send, aux, sizeof(int));

    memcpy(send + sizeof(int), ouput_tfs, (size_t)read_bytes);
    slait_write(fcli, send, sizeof(int) + (size_t)read_bytes);
}

void tfs_thread_write(slave_t *slave) {

    char buffer[SIZE];
    memset(buffer, '\0', SIZE);   

    int session_id = slave->session_id;
    int fhandler = slave->request.fhandler;
    size_t len = slave->request.len;
    char to_write[len];
    memset(to_write, '\0', len);
    memcpy(to_write, slave->request.to_write, len);

    free(slave->request.to_write);
 
    pthread_rwlock_rdlock(&read_lock);
    int fcli = sessions[session_id].fhandler; // this fhandler belongs to the client itself
    pthread_rwlock_unlock(&read_lock);
    
    if (fhandler == -1) {
        slait_write(fcli, buffer, sizeof(int));
        return;
    }
    ssize_t written = tfs_write(fhandler, to_write, len);

    if (written < 0) {
        printf("[ERROR - SERVER] Error writing: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } 

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", (int)written);
    slait_write(fcli, buffer, sizeof(int));
}

void tfs_thread_shutdown_after_all_close(slave_t *slave) {

    char buffer[SIZE];
    int session_id = slave->session_id;

    int ret = tfs_destroy_after_all_closed();

    if (ret == -1) {
        printf("[ERROR - SERVER] Destroy failed\n");
        exit(EXIT_FAILURE);
    }

    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", ret);

    int fcli = sessions[session_id].fhandler;
    slait_write(fcli, buffer, sizeof(int));

    printf("[INFO - SERVER] THREAD SHUTDOWN RETURNING FROM OPS\n");
    
    ssize_t write_size = write(fcli, buffer, sizeof(int));
    if (write_size == -1) {
        printf("[ERROR - SERVER] Error writing: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

void solve_request(slave_t *slave) {

    printf("[INFO - SERVER] Solving request from : %d\n", slave->session_id);

    while(1) {

        pthread_mutex_lock(&slave->slave_mutex);

        while(!slave->wake_up) {
            pthread_cond_wait(&slave->work_cond, &slave->slave_mutex);
        }
        slave->wake_up = 0;

        pthread_mutex_unlock(&slave->slave_mutex);

        int command = slave->request.op_code;

        printf("[INFO - SERVER] (From : %d) Solve request number %d\n", slave->session_id, command);

        switch(command) {
            // nao esquecer de settar a wake_up var a 0 again
            case (TFS_OP_CODE_MOUNT):
                tfs_thread_mount(slave);
            break;

            case (TFS_OP_CODE_UNMOUNT):
                tfs_thread_unmount(slave);
            break;

            case (TFS_OP_CODE_OPEN):
                tfs_thread_open(slave);
            break;

            case (TFS_OP_CODE_CLOSE):
                tfs_thread_close(slave);          
            break;

            case (TFS_OP_CODE_WRITE):
                tfs_thread_write(slave);
            break;

            case (TFS_OP_CODE_READ):
                tfs_thread_read(slave);              
            break;

            case (TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED):
                tfs_thread_shutdown_after_all_close(slave);
            break;

            default:
                printf("[tfs_server] Switch case : Command %c: No correspondance\n", command);
            break;
            
        }
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

        slaves[i].request.op_code = -1;
        slaves[i].request.fcli = -1;
        slaves[i].request.fhandler = -1;
        slaves[i].request.flags = -1;
        slaves[i].request.len = 0;
        memset(slaves[i].request.name, '\0', NAME_SIZE);
        
        if (pthread_mutex_init(&slaves[i].slave_mutex, NULL) != 0) {
            printf("[ERROR - SERVER] Error creating mutex : %s\n", strerror(errno));
            return -1;
        }
        if (pthread_rwlock_init(&slaves[i].slave_rdlock, NULL) != 0) {
            printf("[ERROR - SERVER] Error creating rwlock : %s\n", strerror(errno));
            return -1;
        }
        slaves[i].session_id = i;
        slaves[i].wake_up = 0;
        if (pthread_cond_init(&slaves[i].work_cond, NULL) != 0) {
            printf("[ERROR - SERVER] Error creating cond var : %s\n", strerror(errno));
            return -1;
        }
    }

    for (int i = 0; i < S; i++) {
        if (pthread_create(&slaves[i].tid, NULL, (void *)solve_request, (void *)&(slaves[i])) != 0) {
            printf("[ERROR - SERVER] Error creating thread : %s\n", strerror(errno));
            return -1;
        }
    }

    if ((pthread_create(&counter_thread, NULL, (void *)counter_ops, NULL)) == -1) exit(EXIT_FAILURE);

    return 0;
}

int main(int argc, char **argv) {

    int command;
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
        printf("[ERROR - SERVER]: unlink(%s) failed: %s\n", server_pipe, strerror(errno));
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

    if (signal(SIGINT, immortal) == SIG_ERR) {
        printf("[ERROR - SERVER] %s\n", strerror(errno));
        raise(SIGINT);
    }
        
    if (tfs_init() == -1) {
        printf("[ERROR - SERVER] TFS init failed\n");
        exit(EXIT_FAILURE);
    }

    while(1){
        if ((fserv = open(server_pipe, O_RDONLY)) >= 0) {
            break;
        }
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
            printf("[ERROR - SERVER] Reading : %s\n", strerror(errno));
            return -1;
        }

        command = atoi(buffer);

        switch(command) {
            case (TFS_OP_CODE_MOUNT):
                tfs_handle_mount(name);
            break;

            case (TFS_OP_CODE_UNMOUNT):
                tfs_handle_unmount();
            break;

            case (TFS_OP_CODE_OPEN):
                tfs_handle_open();

            break;

            case (TFS_OP_CODE_CLOSE):
                tfs_handle_close();          

            break;

            case (TFS_OP_CODE_WRITE):
                tfs_handle_write();

            break;

            case (TFS_OP_CODE_READ):
                tfs_handle_read();              

            break;

            case (TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED):
                tfs_handle_shutdown_after_all_close();
            break;

            default:
                printf("[tfs_server] Switch case : Command %c: No correspondance\n", command);
            break;
        }
    }   
    return 0;
}