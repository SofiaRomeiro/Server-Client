#include "common.h"

// fazer para o write tambem
/*
Sleep + Wait
*/
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

        printf("[INFO - SLAIT] Written count = %ld || len = %ld\n", written_tfs, len);

        if (written_count >= len)
            break;
    }
    return written_tfs;
}