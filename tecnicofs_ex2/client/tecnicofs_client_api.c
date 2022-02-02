#include "tecnicofs_client_api.h"

#define PERMISSIONS 0777
#define BUFFER_SIZE 40
#define NAME_SIZE 40

static int fcli;
static int fserv;
static int session_id = -1;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {

    printf("[INFO - API] (%d) Calling api mount...\n", getpid());

    char buffer[sizeof(char) + NAME_SIZE];
    memset(buffer, '\0', sizeof(buffer));

    // WRITE MESSAGE TO SERVER 

    // OP_CODE
    char code = TFS_OP_CODE_MOUNT + '0';
    memcpy(buffer, &code, sizeof(char));
    // NAME FOR CLIENT PIPE
    memcpy(buffer + sizeof(char), client_pipe_path, NAME_SIZE);

    // SEND MESSAGE
    if ((fserv = open(server_pipe_path, O_WRONLY)) < 0) {
        printf("[ERROR - API] Error opening server: %s\n", strerror(errno));
        return -1;
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
        printf("[ERROR - API] Invalid session id : %d\n", session_id);
        return -1;
    }

    return 0;
}

int tfs_unmount() {

    printf("[INFO - API] (%d) Calling api unmount...\n", getpid());
    
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

    printf("[INFO - API] Client %d is dead\n", getpid());

    return 0;  
}

int tfs_open(char const *name, int flags) {

    printf("[INFO - API] (%d) Calling api open...\n", getpid());
    
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

    if (size_written < 0) {
        printf("[ERROR - API] Error writing : %s\n", strerror(errno));
    }

    printf("[INFO API] Writting open args...\n");

    memset(buffer, '\0', sizeof(buffer));

    ssize_t ret = slait(buffer, sizeof(int), fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    }

    printf("[INFO API] Readed open response...\n");

    int fhandler = atoi(buffer);
    if (fhandler < 0) return -1;

    return fhandler;
}

int tfs_close(int fhandle) {

    printf("[INFO - API] (%d) Calling api close...\n", getpid());

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

    printf("[INFO - API] (%d) Calling api write...\n", getpid());
    
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

    printf("[INFO - API] (%d) Calling api read...\n", getpid());

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

    ssize_t ret = slait(buffer_c, sizeof(int), fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    }  

    memset(aux, '\0', sizeof(aux));
    memcpy(aux, buffer_c, sizeof(int));

    size_t read_code = (size_t) atoi(aux);

    memset(buffer_c, '\0', sizeof(buffer_c));
    ret = slait(buffer_c, read_code, fcli);
    if (ret == -1) {
        printf("[ERROR - API] Error reading : %s\n", strerror(errno));
    }  

    memcpy(buffer, buffer_c, read_code);

    // READ ANSWER
    return (ssize_t)read_code;
}

int tfs_shutdown_after_all_closed() {

    char buffer[sizeof(char) + sizeof(int)];
    memset(buffer, '\0', sizeof(buffer));
    char aux[sizeof(int)];
    memset(aux, '\0', sizeof(aux));

    // OP_CODE
    char code = TFS_OP_CODE_READ + '0';
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

    return -1;
}
