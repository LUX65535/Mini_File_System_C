#include "DISK_driver_problem1.h"
#include "DISK_driver_problem3.h"

int seekExamImpl(int fd, int offset) {
    int file = examFileIndex(fd);
    if (file < 0) return ERR_FD_NOT_OPEN;

    int length = fat[file].file_length;
    int status = 0;
    if (offset < 0) offset += length;
    if (offset < 0) {
        offset = 0;
        status = ERR_OUT_OF_LOWER_BOUNDS;
    } else if (offset > length) {
        offset = length;
        status = ERR_OUT_OF_UPPER_BOUNDS;
    }
    open_files[fd].offset = offset;
    open_files[fd].seeked = true;
    return status;
}
