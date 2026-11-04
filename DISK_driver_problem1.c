#include "DISK_driver_problem1.h"

#include <stdlib.h>
#include <string.h>

struct OpenFile open_files[MAX_OPENFILES];
bool initialized;

void resetExamFiles(void) {
    for (int i = 0; i < MAX_OPENFILES; i++) {
        free(open_files[i].pipe_buffer);
        open_files[i] = (struct OpenFile){ .fat_index = -1 };
    }
    initialized = true;
}

int examFileIndex(int fd) {
    return initialized && fd >= 0 && fd < MAX_OPENFILES ? open_files[fd].fat_index : -1;
}

int openExamImpl(int fd, char *name) {
    if (fd < 0 || fd >= MAX_OPENFILES) return ERR_FD_NOT_OPEN;
    if (!initialized) resetExamFiles();
    if (open_files[fd].fat_index >= 0) return ERR_FD_OPEN;
    int file = diskOpenEntry(name);
    if (file < 0) return ERR_NO_SPACE;
    open_files[fd] = (struct OpenFile){ .fat_index = file };
    return 0;
}

int readExamImpl(int fd, char **pp_buf) {
    if (!pp_buf) return ERR_NO_SPACE;
    *pp_buf = NULL;
    int file = examFileIndex(fd);
    if (file < 0) return ERR_FD_NOT_OPEN;

    int offset = open_files[fd].seeked ? open_files[fd].offset : 0;
    size_t available = (size_t)(fat[file].file_length - offset);
    char *copy = malloc(available + 1);
    if (!copy) return ERR_NO_SPACE;
    int read = diskReadBytes(file, offset, copy, available);
    if (read < 0) {
        free(copy);
        return read;
    }
    copy[read] = '\0';
    open_files[fd].offset = offset + read;
    *pp_buf = copy;
    return read;
}

int writeExamImpl(int fd, char *str) {
    int file = examFileIndex(fd);
    if (file < 0) return ERR_FD_NOT_OPEN;
    if (!str) return ERR_NO_SPACE;

    int offset = open_files[fd].seeked ? open_files[fd].offset : 0;
    int written = diskWriteBytes(file, offset, str, strlen(str));
    if (written >= 0) open_files[fd].offset = offset + written;
    return written;
}
