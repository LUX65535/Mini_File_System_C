#include "DISK_driver_problem1.h"
#include "DISK_driver_problem4.h"

#include <stdlib.h>
#include <string.h>

int readpipeExamImpl(int fd) {
    if (examFileIndex(fd) < 0) return ERR_FD_NOT_OPEN;
    char *content;
    int status = readExamImpl(fd, &content);
    if (status < 0) return status;
    free(open_files[fd].pipe_buffer);
    open_files[fd].pipe_buffer = content;
    return 0;
}

char *acceptPipeInput(int fd) {
    if (examFileIndex(fd) < 0 || !open_files[fd].pipe_buffer) return NULL;
    const char *content = open_files[fd].pipe_buffer;
    size_t size = strlen(content) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, content, size);
    return copy;
}
