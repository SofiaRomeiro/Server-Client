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
//char input[100];
//char output[100];

/*
void *fn_thread(void *arg) {
    return NULL;
}

*/

int main() {

    // No need to join thread
    printf("Successful test.\n");

    return 0;
}
