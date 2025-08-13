
#include <stdlib.h>
#include <string.h>

#include "DISK_driver.h"
#include "DISK_driver_problem1.h"
#include "DISK_driver_problem2.h"
#include "DISK_driver_problem3.h"


int seekExamImpl(int fd, int offset) {
    int err = 0;
    int active_file_id = open_files[fd].fat_index;
    if (!initialized || active_file_id < 0) return ERR_FD_NOT_OPEN;
    if (offset < 0) {
        offset = fat[active_file_id].file_length + offset;
        if (offset < 0) {
            offset = 0;
            err = ERR_OUT_OF_LOWER_BOUNDS;
        }
    }
    if (offset > fat[active_file_id].file_length) {
        offset = fat[active_file_id].file_length;
        err = ERR_OUT_OF_UPPER_BOUNDS;
    }
    open_files[fd].offset = offset;
    /*
     * tell system that the file has been seeked,
     * this will affect the reading and writing behavior
     */
    open_files[fd].seeked = true;
    return err;
}

