
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "DISK_driver.h"
#include "DISK_driver_problem1.h"


/*
 * import functions and variables
 */
extern int partitionFolderExists(void);
extern void updateFATOnDisk(void);
extern void fseekToDataSection(FILE *file);
extern void fseekToBlock(FILE *file, int blockID);
extern void fseekToNextFreeBlock(FILE *file);
extern FILE* partitionFile;
extern char filename[150];



/*
 * open file id is just a map
 * of active file id
 */
struct OpenFile  open_files[MAX_OPENFILES];

/*
 * openfiles need to be initialized at
 * the first run
 */
bool initialized = false;


static void seekToStart(int fd);
static void syncToDisk();


static void initialize_open_files() {
    unsigned int i;
    for (i = 0; i < MAX_OPENFILES; ++i) {
        open_files[i].fat_index = -1;
        open_files[i].offset = 0;
        open_files[i].seeked = false;
        open_files[i].pipe_buffer = NULL;
    }
}

/*
 * this function is to check file
 * after operation, for debuggin
 */
static void check_file(const char* op) {
    FILE* f = fopen(filename, "r");
    int c;
    printf("%s, File content is: \n", op);
    while ((c = fgetc(f)) != EOF) {
        printf("%c", (char)c);
    }
    printf("\n");
    fclose(f);
}

/*
 * create a file, if file not exists,
 * otherwise return the existing file handle
 */
static int createFile(char *name){
    // Lookup on the FAT
    int i, FATindex = -1;

    // check if file already exists
    for (i = 0; i < MAX_FILES; i++) {
        if (fat[i].filename == NULL) {
            if (FATindex < 0) {
                FATindex = i;
            }
        } else if (strcmp(fat[i].filename, name) == 0) {
            /* file already exists*/
            return i;
        }

    }
    if(FATindex == MAX_FILES && fat[MAX_FILES -1].filename != NULL){
        return -1;
    }
    /* process create new file*/
    if(fat[FATindex].filename == NULL){
        // Make a new file
        fat[FATindex].filename = malloc(500);
        strcpy(fat[FATindex].filename, name);
        fat[FATindex].file_length = 0;
        // check_file("Before sync to disk");
        syncToDisk();
    }
    // check_file("After create file");
    return FATindex;
}

/*
 * the reason to write this function
 * is to update fat while keeping the
 * file system data, because the size of new
 * fat may be less than the origin fat,
 * in this case, data sector can not be
 * located correctly.
 */
static void syncToDisk()
{
    int i, l, c;
    char* tempBuf = malloc(currentPartition.total_blocks * currentPartition.block_size);

    i = 0;
    // Update the FAT on disk
    partitionFile = fopen(filename, "r+");
    /*
     * first step is to backup data sector
     */
    fseekToDataSection(partitionFile);
    while ((c = fgetc(partitionFile)) != EOF) {
        tempBuf[i++] = (char)c;
    }
    fclose(partitionFile);

    /* second step is to update fat,
     * then truncate the file*/
    l = i;
    partitionFile = fopen(filename, "w");

    updateFATOnDisk();
    fclose(partitionFile);

    /* last step is to copy the
     * backup data back to file*/

    partitionFile = fopen(filename, "r+");
    fseekToDataSection(partitionFile);
    for (i = 0; i < l; ++i) {
        fputc(tempBuf[i], partitionFile);
    }
    fclose(partitionFile);
}

/*
 * return 0 if success
 * return -1 if not enough blocks
 * return -2 if already opened by other file
 */
int openExamImpl(int fd, char *name){
    int fat_index;
    if (!initialized) {
        initialize_open_files();
        initialized = true;
    }
    if (open_files[fd].fat_index >= 0) return -2;
    fat_index = createFile(name);
    if (fat_index < 0) {
        return -1;
    }
    open_files[fd].fat_index = fat_index;
    open_files[fd].offset = 0;
    open_files[fd].seeked = false;
    open_files[fd].pipe_buffer = NULL;
    return 0;
}

static int myReadBlock(int fat_index, int blockToRead, char* buffer) {
    int i, block_id;
    FILE* filePtr;

    block_id = fat[fat_index].blockPtrs[blockToRead];
    if (block_id < 0) {
        return -1;
    }
    filePtr = fopen(filename, "r+");
    fseekToBlock(filePtr, block_id);
    for (i = 0; i < currentPartition.block_size; i++) {
        char c = fgetc(filePtr);
        if(c == '0'){
            buffer[i] = '\0';
            break;
        }
        buffer[i] = c;
    }
    buffer[i] = '\0';
    fclose(filePtr);
    return 0;
}

static int myWriteBlock(int fat_index, int blockToWrite, char*data) {
    bool block_allcated = false;
    int block_id;
    // Get the fat_index pointer
    FILE *filePtr = NULL;

    block_id = fat[fat_index].blockPtrs[blockToWrite];

    filePtr = fopen(filename, "r+");
    if (block_id < 0) {
        // Seek to next free block
        fseekToNextFreeBlock(filePtr);
        block_allcated = true;
    } else {
        fseekToBlock(filePtr, block_id);
    }
    // Writing the block char by char into the buffer
    for (int i = 0; i < currentPartition.block_size; i++) {
        char c = *(data+i);
        if(c == 0)
            c = '0';
        fputc(c, filePtr);
    }
    fclose(filePtr);

// Updating the fat_index's blockPtr array
    if (block_allcated) {
        fat[fat_index].blockPtrs[blockToWrite] = currentPartition.last_free_block;
        currentPartition.last_free_block += 1;
        blockToWrite += 1;
        if(blockToWrite < MAX_BLOCKS)
            fat[fat_index].blockPtrs[blockToWrite] = -1;
    }
    return 0;
}

/*
 * read implementation
 */
int readExamImpl(int fd, char** pp_buf) {
    int err, pos, total, rd, blockToRead, offsetInBlock;
    char* block, *buf;
    int fat_index = open_files[fd].fat_index;


    if (!initialized || fat_index < 0) return ERR_FD_NOT_OPEN;

    if (!open_files[fd].seeked) {
        seekToStart(fd);
    }

    pos = 0;
    total = fat[fat_index].file_length - open_files[fd].offset;
    if (total <= 0) return 0;

    block = malloc(currentPartition.block_size);
    buf = malloc(total + 1);
    blockToRead = open_files[fd].offset / currentPartition.block_size;
    offsetInBlock = open_files[fd].offset % currentPartition.block_size;
    while (pos < total) {
        rd = currentPartition.block_size - offsetInBlock;
        if (rd > total - pos) {
            rd = total - pos;
        }
        err = myReadBlock(fat_index, blockToRead, block);
        if (err < 0) {
            free(block);
            free(buf);
            return -1;
        }
        memcpy(buf + pos, block + offsetInBlock, rd);
        pos += rd;
        open_files[fd].offset += rd;
        blockToRead += 1;
        offsetInBlock = 0;
    }
    buf[pos] = 0;
    free(block);
    *pp_buf = buf;
    return (int)pos;
}

int writeExamImpl(int fd, char* str) {
    int err, pos = 0, total, wr, blockToWrite, offsetInBlock;
    int active_file_id = open_files[fd].fat_index;
    bool size_changed = false;
    char* block_buf = NULL;

    if (!initialized || active_file_id < 0) return ERR_FD_NOT_OPEN;

    if (!open_files[fd].seeked) {
        seekToStart(fd);
    }

    total = strlen(str);
    if (!total) return 0;

    blockToWrite = open_files[fd].offset / currentPartition.block_size;
    offsetInBlock = open_files[fd].offset % currentPartition.block_size;
    while (pos < total) {
        wr = currentPartition.block_size - offsetInBlock;
        if (wr > total - pos) {
            wr = total - pos;
        }
        if (wr == currentPartition.block_size) {
            err = myWriteBlock(active_file_id, blockToWrite, str + pos);
            if (err < 0) {
                free(block_buf);
                return pos;
            }
        } else {
            if (!block_buf) {
                block_buf = calloc(1, currentPartition.block_size);
            } else {
                memset(block_buf, 0,  currentPartition.block_size);
            }

            if (offsetInBlock != 0) {
                err = myReadBlock(active_file_id, blockToWrite, block_buf);
                if (err < 0) {
                    free(block_buf);
                    return -1;
                }
            }
            memcpy(block_buf + offsetInBlock, str + pos, wr);
            err = myWriteBlock(active_file_id, blockToWrite, block_buf);
            if (err < 0) {
                free(block_buf);
                return pos;
            }
        }
        pos += wr;
        open_files[fd].offset += wr;
        if (open_files[fd].offset > fat[active_file_id].file_length) {
            fat[active_file_id].file_length = open_files[fd].offset;
            size_changed = true;
        }
        blockToWrite += 1;
        offsetInBlock = 0;
    }
    if (size_changed) {
        syncToDisk();
    }
    free(block_buf);
    return pos;
}

static void seekToStart(int fd) {
    open_files[fd].offset = 0;
}
