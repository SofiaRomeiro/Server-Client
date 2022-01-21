#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/*  Crie e experimente variações multi-threaded deste teste.

    Por exemplo, componha um teste em que duas ou mais threads executam um ciclo em
    que, a cada iteração, abrem, lêem/escrevem e fecham ficheiros; no entanto,
    concorrentemente, a tarefa inicial chama tfs_destroy_after_all_closed.
*/

#define N 3

//int closed_file = 0;
//int f;
char input[100];
char output[100];

//pthread_mutex_t mutex;

void *fn_thread(void *arg) {

    char *path = ((char *)arg);

    printf("Read...\n");

    int fh = tfs_open(path, TFS_O_CREAT);
    assert(tfs_write(fh, input, sizeof(input)) != -1);
    assert(tfs_close(fh) != -1);

    sleep(2);

    printf("Write...\n");
    fh = tfs_open(path, 0);
    if (fh == -1) return NULL;
    tfs_read(fh, output, sizeof(output));
    tfs_close(fh);

    return NULL;
}



int main() {

    pthread_t tids[N];

    char *path = "/f1";

    memset(input, 'S', sizeof(input));
    memset(output, '\0', sizeof(output));

    assert(tfs_init() != -1);

    for (int i = 0; i < N; i++) {
        assert(pthread_create(&tids[i], NULL, fn_thread, (void*)path) == 0);
    }

    sleep(2);

    assert(tfs_destroy_after_all_closed() == 0);

    for (int i = 0; i < N; i++) {
        pthread_join(tids[i], NULL);
    }

    printf("Successful test.\n");

    return 0;
}
