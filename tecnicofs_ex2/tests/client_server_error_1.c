#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*  This test pretends to test if the server is returning the errors without
    dying. To do that, this test must succeed and the server remain alive. 
*/

#define SIZE 1028
#define BLOCK_SIZE 1024

int main(int argc, char **argv) {

    char str[SIZE];
    char *path = "/f1";

    memset(str, 'R', SIZE);

    int f;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_unmount() == -1);

    f = tfs_open(path, TFS_O_CREAT);
    assert(f == -1);

    assert(tfs_mount(argv[1], argv[2]) == 0);

    assert(tfs_close(f) == -1);

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str, strlen(str));
    assert(r == BLOCK_SIZE);

    assert(tfs_close(f) != -1);

    assert(tfs_unmount() == 0);

    printf("Successful test.\n");

    return 0;
}
