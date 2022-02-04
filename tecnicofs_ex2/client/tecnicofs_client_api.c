#include "tecnicofs_client_api.h"

#define PERMISSIONS 0777
#define BUFFER_SIZE 40
#define NAME_SIZE 40

static int fcli;
static int fserv;
static int session_id = -1;

ssize_t slait(char *buffer_c, size_t len, int fh) {

    ssize_t written_count = 0, written_tfs = 0;

    while(1) {

        written_tfs = read(fh, buffer_c + written_count, len);  

        if (written_tfs == -1) {
            printf("[ERROR - SLAIT] Error reading file, %s\n", strerror(errno));
            return written_tfs;
        }

        else if (written_tfs == 0) {
            printf("[INFO - SLAIT] Slait EOF\n");
            return written_tfs;
        }

        written_count += written_tfs;

        if (written_count >= len)
            break;
    }
    return written_tfs;
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    //printf("[INFO - API] (%d) Calling api mount...\n", getpid());

    char buffer[sizeof(char) + NAME_SIZE];
    memset(buffer, '\0', sizeof(buffer));

    // WRITE MESSAGE TO SERVER 

    // OP_CODE
    char code = TFS_OP_CODE_MOUNT + '0';
    memcpy(buffer, &code, sizeof(char));
    // NAME FOR CLIENT PIPE
    memcpy(buffer + sizeof(char), client_pipe_path, NAME_SIZE);

    // SEND MESSAGE
    while(1) {
        if ((fserv = open(server_pipe_path, O_WRONLY)) >= 0) {
            break;
        }
    }
    

    if (write(fserv, buffer, sizeof(char) + NAME_SIZE) == -1) {
        printf("[ERROR - API] Error writing : %s\n", strerror(errno));
        return -1;
    }        

    memset(buffer, '\0', sizeof(buffer));

    // CREATE AND OPEN CLIENT'S PIPE
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        printf("[ERROR - SERVER]: unlink(%s) failed: %s\n", client_pipe_path, strerror(errno));
        return -1;
    }

    if (mkfifo(client_pipe_path, PERMISSIONS) != 0) { // criar named pipe - client
        printf("[ERROR - SERVER] Mkfifo failed : %s\n", strerror(errno));
        return -1;
    }

    if ((fcli = open(client_pipe_path, O_RDONLY)) < 0) {
        printf("[ERROR - SERVER] Error opening client : %s\n", strerror(errno));
        return -1;
    } 

    // RECEIVE SERVER RESPONSE
    ssize_t ret = slait(buffer, sizeof(int), fcli); 
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
        return -1;
    }  

    session_id = atoi(buffer); 
    if (session_id < 0) {
        printf("[ERROR - API] (%d) Invalid session id : %d\n", getpid(), session_id);
        return -1;
    }

    return 0;
}

int tfs_unmount() {

    //printf("[INFO - API] (%d) Calling api unmount...\n", getpid());
    if (session_id == -1) {
        printf("[INFO - API] Unmount : Invalid session id\n");
        return -1;
    } 
    char buffer[sizeof(char) + sizeof(int)];
    char aux[sizeof(char) + sizeof(int)];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_UNMOUNT + '0';
    memcpy(buffer, &code, sizeof(char));  

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer + sizeof(char), aux, sizeof(int));

    // SEND MSG TO SERVER
    ssize_t size_written = write(fserv, buffer, sizeof(char) + sizeof(int));

    if (size_written < 0) {
        printf("[ERROR - API] Error on writing : %s\n", strerror(errno));
        return -1;
    }

    // CLOSE SERVER AND CLIENT PIPE
    if (close(fserv) == -1) {
        printf("[ERROR - API] Error on writing : %s\n", strerror(errno));
        return -1;
    }
    if (close(fcli) == -1) {
        printf("[ERROR - API] Error on writing : %s\n", strerror(errno));
        return -1;
    }

    //printf("[INFO - API] Client %d is dead\n", getpid());

    return 0;  
}

int tfs_open(char const *name, int flags) {

    //printf("[INFO - API] (%d) Calling api open...\n", getpid());

    if (session_id == -1) {
        printf("[INFO - API] Open : Invalid session id\n");
        return -1;
    } 
    
    char buffer[sizeof(char) + sizeof(int) + NAME_SIZE + sizeof(int)];
    memset(buffer, '\0', sizeof(buffer));
    char session_id_s[sizeof(int)];
    memset(session_id_s, '\0', sizeof(session_id_s));
    char filename[BUFFER_SIZE];
    memset(filename, '\0', sizeof(filename));
    memcpy(filename, name, sizeof(filename));
    size_t len = 0;

    // OP_CODE
    char code = TFS_OP_CODE_OPEN + '0';
    memcpy(buffer, &code, sizeof(char));
    len += sizeof(char);  

    // SESSION_ID
    sprintf(session_id_s, "%d", session_id);
    memcpy(buffer + len, session_id_s, sizeof(int));
    len += sizeof(int);
    
    // NAME [40]
    memcpy(buffer + len, filename, sizeof(filename));
    len += sizeof(filename);

    // FLAGS - despite being a combination, only one number 
    // is received so it can be converted to char immediately
    char flags_c = (char) (flags + '0');
    memcpy(buffer + len, &flags_c, sizeof(char));
    len += sizeof(char);

    ssize_t size_written = write(fserv, buffer, len);

    if (size_written < 0 || size_written < len) {
        printf("[ERROR - API] Error writing : %s\n", strerror(errno));
        return -1;
    }

    //printf("[INFO API] Writting open args...\n");

    memset(buffer, '\0', sizeof(buffer));

    ssize_t ret = slait(buffer, sizeof(int), fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    }

    //printf("[INFO API] Readed open response...\n");

    int fhandler = atoi(buffer);
    if (fhandler < 0) return -1;

    return fhandler;
}

int tfs_close(int fhandle) {

    //printf("[INFO - API] (%d) Calling api close...\n", getpid());
    if (session_id == -1) {
        printf("[INFO - API] Close : Invalid session id\n");
        return -1;
    } 

    size_t len = 0;
    char buffer[sizeof(char) + 2 * sizeof(int)]; // 2 * sizeof(int)??
    char aux[sizeof(int)];
    memset(buffer, '\0', sizeof(buffer));
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_CLOSE + '0';
    memcpy(buffer, &code, sizeof(char));  
    len += sizeof(char);

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer + len, aux, sizeof(int));
    len += sizeof(int);

    // FHANDLE
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%d", fhandle);
    memcpy(buffer + len, aux, sizeof(int));  
    len += sizeof(int);

    // SEND MESSAGE
    ssize_t size_written = write(fserv, buffer, len);
    if (size_written < 0) {
        printf("[ERROR - API] tfs_open error on writing: %s\n", strerror(errno));
        return -1;
    }

    // READ ANSWER
    memset(aux, '\0', sizeof(aux));
    ssize_t ret = slait(aux, sizeof(int), fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    } 

    int close = atoi(aux);
    if (close < 0) return -1;

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {

    //printf("[INFO - API] (%d) Calling api write...\n", getpid());
    if (session_id == -1) {
        printf("[INFO - API] Write : Invalid session id\n");
        return -1;
    } 
    
    //size_t buffer_size = 1 + 4 + 4 + 4 + len + 1;
    size_t buffer_size = sizeof(char) + (3 * sizeof(int)) + len + sizeof(char);
    size_t offset = 0;
    char buffer_c[buffer_size];
    memset(buffer_c, '\0', sizeof(buffer));
    char aux[2 * sizeof(int) + sizeof(char)]; //int + int + char??
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_WRITE + '0';
    memcpy(buffer_c, &code, sizeof(char));
    offset += sizeof(char);

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer_c + offset, aux, sizeof(int));
    offset += sizeof(int);

    // FHANDLE    
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%d", fhandle);
    memcpy(buffer_c + offset, aux, sizeof(int));
    offset += sizeof(int);

    // LEN
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%ld", len);
    memcpy(buffer_c + offset, aux, sizeof(int));
    offset += sizeof(int);

    // BUFFER WITH CONTENT
    memcpy(buffer_c + offset, buffer, len); // ??
    offset += len;

    // SEND MSG TO SERVER
    ssize_t size_written = write(fserv, buffer_c, offset);

    if (size_written < 0) {
        printf("[ERROR - API] Error writing : %s\n", strerror(errno));
        return -1;
    }

    memset(buffer_c, '\0', sizeof(buffer_c));

    ssize_t ret = slait(buffer_c, len, fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
        return -1;
    }

    int written = atoi(buffer_c);
    if (written < 0) return -1;

    return written;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    //printf("[INFO - API] (%d) Calling api read...\n", getpid());
    if (session_id == -1) {
        printf("[INFO - API] Read : Invalid session id\n");
        return -1;
    } 

    size_t buffer_size = (2 * sizeof(char)) + (3 * sizeof(int));
    size_t offset = 0;
    char buffer_c[buffer_size];
    memset(buffer_c, '\0', sizeof(buffer_c));
    char aux[2 * sizeof(int) + sizeof(char)]; //int + int + char??
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_READ + '0';
    memcpy(buffer_c, &code, sizeof(char)); 
    offset += sizeof(char);

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer_c + offset, aux, sizeof(int));
    offset += sizeof(int);

    // FHANDLE
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%d", fhandle);
    memcpy(buffer_c + offset, aux, sizeof(int));
    offset += sizeof(int);

    // LEN
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "%ld", len);
    memcpy(buffer_c + offset, aux, sizeof(int));
    offset += sizeof(int);

    // SEND MSG TO SERVER

    ssize_t size_written = write(fserv, buffer_c, offset);
    if (size_written < 0) {
        printf("[ERROR - API] Error : %s\n", strerror(errno));
    }

    memset(buffer_c, '\0', sizeof(buffer_c));

    // RECEIVE MESSAGE FROM SERVER

    ssize_t ret = slait(buffer_c, sizeof(int), fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    }  

    memset(aux, '\0', sizeof(aux));
    memcpy(aux, buffer_c, sizeof(int));

    // in case of returning error from server
    ssize_t read_code = (ssize_t) atoi(aux);

    if (read_code < 0) {
        printf("[ERROR -API] Error reading from tfs system\n");
        return -1;
    }

    // in case of sucess, copy directly to void *buffer
    //memset(buffer_c, '\0', sizeof(buffer_c));
    ret = slait(buffer, (size_t)read_code, fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
        return -1;
    }  

    //memcpy(buffer, buffer_c, read_code);

    // READ ANSWER
    return (ssize_t)read_code;
}

int tfs_shutdown_after_all_closed() {

    if (session_id == -1) {
        printf("[INFO - API] Shutdown : Invalid session id\n");
        return -1;
    } 

    char buffer[sizeof(char) + sizeof(int)];
    memset(buffer, '\0', sizeof(buffer));
    char aux[sizeof(int)];
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED + '0';
    memcpy(buffer, &code, sizeof(char)); 

    // SESSION_ID
    sprintf(aux, "%d", session_id);
    memcpy(buffer + sizeof(char), aux, sizeof(int));

    ssize_t size_written = write(fserv, buffer, sizeof(buffer));

    if (size_written < sizeof(buffer)) {
        printf("[ERROR - API] tfs_open error on writing\n");
    }

    memset(aux, '\0', sizeof(aux));
    
    ssize_t ret = slait(aux, sizeof(int), fcli);
    
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    } 

    int ret_aux = atoi(aux);
    if (ret_aux < 0) return -1;

    return 0;
}