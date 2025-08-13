
#include <stdlib.h>
#include "DISK_driver.h"
#include "DISK_driver_problem1.h"
#include "DISK_driver_problem2.h"

int closeExamImpl(int fd) {
    // Checking if the file is not already open
    int fat_index;
    if (!initialized || fd < 0) return ERR_FD_NOT_OPEN;
    fat_index = open_files[fd].fat_index;
    if (fat_index < 0) return ERR_FD_NOT_OPEN;
    open_files[fd].fat_index = -1;
    free(open_files[fd].pipe_buffer);
    open_files[fd].pipe_buffer = NULL;
    return 0;
}