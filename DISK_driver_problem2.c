#include "DISK_driver_problem1.h"
#include "DISK_driver_problem2.h"

#include <stdlib.h>

int closeExamImpl(int fd) {
    if (examFileIndex(fd) < 0) return ERR_FD_NOT_OPEN;
    free(open_files[fd].pipe_buffer);
    open_files[fd] = (struct OpenFile){ .fat_index = -1 };
    return 0;
}
