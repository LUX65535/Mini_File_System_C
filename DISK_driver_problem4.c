
#include <stdlib.h>
#include <string.h>

#include "DISK_driver.h"
#include "DISK_driver_problem1.h"
#include "DISK_driver_problem2.h"
#include "DISK_driver_problem3.h"
#include "DISK_driver_problem4.h"


int readpipeExamImpl(int fd) {
    int err, fat_index;
    fat_index = open_files[fd].fat_index;
    if (!initialized || fat_index < 0) return ERR_FD_NOT_OPEN;

    free(open_files[fd].pipe_buffer);
    err = readExamImpl(fd, &open_files[fd].pipe_buffer);
    if (err < 0) return err;
    return 0;
}

char* acceptPipeInput(int fd) {
    int fat_index;
    char* content;
    fat_index = open_files[fd].fat_index;
    if (!initialized || fat_index < 0 || !open_files[fd].pipe_buffer) return NULL;
    content = malloc(strlen(open_files[fd].pipe_buffer) + 1);
    strcpy(content, open_files[fd].pipe_buffer);
    content[fat[fat_index].file_length] = 0;
    return content;
}