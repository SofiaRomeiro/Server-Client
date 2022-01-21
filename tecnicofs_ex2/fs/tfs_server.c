#include "operations.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


#define SERVER "/tmp/server"
#define CLIENT "tmp/client"
#define PERMISSIONS 0777
#define SIZE 100

int main(int argc, char **argv) {

    char buffer[SIZE];
    memset(buffer, '\0', SIZE);

    int fcli, fserv;
    ssize_t n;
    char command;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    unlink(SERVER);
    unlink(CLIENT);

    if (mkfifo(SERVER, PERMISSIONS) < 0) {
        return -1;
    }
    if (mkfifo(CLIENT, PERMISSIONS) < 0) {
        return -1;
    }

    if ((fserv = open(SERVER, O_RDONLY)) < 0) return -1;

    if ((fcli = open(CLIENT, O_WRONLY)) < 0) return -1;

    while (1) {
        n = read(fserv, buffer, SIZE);
        if (n <= 0) break;

        command = buffer[0];      

        switch(command) {
            case (TFS_OP_CODE_MOUNT):
            break;

            case (TFS_OP_CODE_UNMOUNT):
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