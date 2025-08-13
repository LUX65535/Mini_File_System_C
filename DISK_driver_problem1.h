//    DISK_driver_problem1.h

#ifndef DISK_DRIVER_PROBLEM1_H
#define DISK_DRIVER_PROBLEM1_H

#include <stdbool.h>

#define ERR_NO_SPACE -1
#define ERR_FD_OPEN -2 /* file id already opened by other file*/
#define ERR_FD_NOT_OPEN -3

/*
 * open file structures
 */
struct OpenFile {
    int fat_index;
    int offset;
    bool seeked;
    char* pipe_buffer;
};

/*
 * export to problem 2 3 4
 */
extern struct OpenFile open_files[MAX_OPENFILES];
extern bool initialized;


/*
 * open exam implementation
 * fd is active file id
 * name is filename
 */
int openExamImpl(int fd, char *name);

/*
 * read exam implementation
 * file id is active file id opened
 * pp buf is to save the result
 */
int readExamImpl(int fd, char** pp_buf);



/*
 * write exam implementation
 * file id is active file id opened
 * str is string to write
 */
int writeExamImpl(int fd, char* str);


#endif