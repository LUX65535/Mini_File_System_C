#ifndef DISK_driver_h
#define DISK_driver_h

#include <stdio.h>

#define MAX_FILES 20
#define MAX_BLOCKS 10
#define MAX_OPENFILES 50

struct PARTITION {
    int block_size;
    int total_blocks;
    int last_free_block;
};

struct FAT {
    char *filename;
    int file_length;
    int blockPtrs[MAX_BLOCKS];
    int current_location;
};

extern struct PARTITION currentPartition;
extern struct FAT fat[MAX_FILES];
extern char *blockBuffer;
extern FILE *active_file_table[MAX_OPENFILES];
extern int active_file_table_owners[MAX_OPENFILES];

void initIO(void);
float getBlockSize(void);
int partition(char *name, int blocksize, int totalblocks);
int mountFS(char *name);
int mountOrCreateFS(char *name, int blocksize, int totalblocks);
int openFile(char *name);
void closeFile(int file);
char *readBlock(int file);
int writeBlock(int file, char *data);

/* Shared byte operations for the kernel shell and descriptor API. */
int diskOpenEntry(const char *name);
int diskReadBytes(int file, int offset, void *output, size_t count);
int diskWriteBytes(int file, int offset, const void *data, size_t count);

/* Read the physical contents without changing any descriptor position. */
int inspectDiskBlock(int block, unsigned char *bytes, size_t capacity);

#endif
