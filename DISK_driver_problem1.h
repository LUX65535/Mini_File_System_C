#ifndef DISK_DRIVER_PROBLEM1_H
#define DISK_DRIVER_PROBLEM1_H

#include <stdbool.h>
#include "DISK_driver.h"

#define ERR_NO_SPACE -1
#define ERR_FD_OPEN -2
#define ERR_FD_NOT_OPEN -3

struct OpenFile {
    int fat_index;
    int offset;
    bool seeked;
    char *pipe_buffer;
};

extern struct OpenFile open_files[MAX_OPENFILES];
extern bool initialized;

void resetExamFiles(void);
int examFileIndex(int fd);
int openExamImpl(int fd, char *name);
int readExamImpl(int fd, char **pp_buf);
int writeExamImpl(int fd, char *str);

#endif
