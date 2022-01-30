#include "common.h"

// fazer para o write tambem
/*
Sleep + Wait
*/
void slait(char *buffer_c, size_t len, int fh) {

    ssize_t written_count = 0, written_tfs = 0;

    while(1) {

        written_tfs = read(fh, buffer_c + written_count, len);  

        if (written_tfs == -1) {
            printf("[ERROR - API] Error reading file\n");
            exit(EXIT_FAILURE);
        }

        else if (written_tfs == 0) {
            printf("[INFO - API] Slait EOF\n");
            return;
        }

        written_count += written_tfs;

        if (written_count >= len)
            break;
    }
}