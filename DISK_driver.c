#define _POSIX_C_SOURCE 200809L

#include "DISK_driver.h"
#include "DISK_driver_problem1.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_NAME 127
#define MAX_PARTITION_BYTES (16u * 1024 * 1024)

struct PARTITION currentPartition = { -1, -1, -1 };
struct FAT fat[MAX_FILES];
char *blockBuffer;
FILE *active_file_table[MAX_OPENFILES];
int active_file_table_owners[MAX_OPENFILES];

static char partitionPath[MAX_NAME + 16];
static unsigned char *partitionData;

static struct FAT emptyEntry(void) {
    struct FAT entry = { .file_length = -1, .current_location = -1 };
    for (int i = 0; i < MAX_BLOCKS; i++) entry.blockPtrs[i] = -1;
    return entry;
}

void initIO(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        free(fat[i].filename);
        fat[i] = emptyEntry();
    }
    for (int i = 0; i < MAX_OPENFILES; i++) {
        if (active_file_table[i]) fclose(active_file_table[i]);
        active_file_table[i] = NULL;
        active_file_table_owners[i] = -1;
    }
    resetExamFiles();
    free(blockBuffer);
    free(partitionData);
    blockBuffer = NULL;
    partitionData = NULL;
    currentPartition = (struct PARTITION){ -1, -1, -1 };
    partitionPath[0] = '\0';
}

float getBlockSize(void) {
    return (float)currentPartition.block_size;
}

static int validName(const char *name) {
    if (!name || !*name || strnlen(name, MAX_NAME + 1) > MAX_NAME ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, "(null)") == 0) return 0;
    for (const unsigned char *c = (const unsigned char *)name; *c; c++) {
        if (*c < 32 || *c == 127 || *c == ',' || *c == '/' || *c == '\\') return 0;
    }
    return 1;
}

static int makePath(char path[sizeof(partitionPath)], const char *name) {
    if (!validName(name)) return 0;
    snprintf(path, sizeof(partitionPath), "PARTITION/%s.txt", name);
    return 1;
}

static size_t diskSize(struct PARTITION disk) {
    if (disk.block_size <= 0 || disk.total_blocks <= 0 ||
        (size_t)disk.block_size > MAX_PARTITION_BYTES / (size_t)disk.total_blocks) return 0;
    return (size_t)disk.block_size * (size_t)disk.total_blocks;
}

static int blocksForLength(int length, int blocksize) {
    return length ? (length - 1) / blocksize + 1 : 0;
}

static int saveDisk(const char *path, struct PARTITION disk, const struct FAT entries[MAX_FILES],
                    const unsigned char *data) {
    char temporary[sizeof(partitionPath) + sizeof(".tmp.XXXXXX")];
    snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path);
    int fd = mkstemp(temporary);
    if (fd < 0) return 0;
    FILE *file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        remove(temporary);
        return 0;
    }

    /* The table changes size as names and block IDs grow. Replace the full image. */
    int ok = fprintf(file, "MFS1 %d, %d, %d\n", disk.block_size, disk.total_blocks, disk.last_free_block) >= 0;
    for (int i = 0; i < MAX_FILES && ok; i++) {
        const struct FAT *entry = &entries[i];
        ok = fprintf(file, "%s, %d\n", entry->filename ? entry->filename : "(null)", entry->file_length) >= 0;
        for (int j = 0; j < MAX_BLOCKS && ok; j++) {
            ok = fprintf(file, j ? "; %d" : " %d", entry->blockPtrs[j]) >= 0;
        }
        if (ok) ok = fputc('\n', file) != EOF;
    }
    size_t size = diskSize(disk);
    if (ok) ok = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) ok = 0;
    if (ok && rename(temporary, path) == 0) return 1;
    remove(temporary);
    return 0;
}

static void installDisk(const char *path, struct PARTITION disk, const struct FAT entries[MAX_FILES],
                        unsigned char *data, char *buffer) {
    initIO();
    strcpy(partitionPath, path);
    currentPartition = disk;
    memcpy(fat, entries, sizeof(fat));
    partitionData = data;
    blockBuffer = buffer;
}

int partition(char *name, int blocksize, int totalblocks) {
    char path[sizeof(partitionPath)];
    struct PARTITION disk = { blocksize, totalblocks, 0 };
    size_t size = diskSize(disk);
    if (!size || !makePath(path, name)) return 0;
    if (mkdir("PARTITION", 0777) != 0 && errno != EEXIST) return 0;
    unsigned char *data = calloc(size, 1);
    char *buffer = malloc((size_t)blocksize + 1);
    struct FAT entries[MAX_FILES];
    for (int i = 0; i < MAX_FILES; i++) entries[i] = emptyEntry();
    if (!data || !buffer || !saveDisk(path, disk, entries, data)) {
        free(data);
        free(buffer);
        return 0;
    }
    installDisk(path, disk, entries, data, buffer);
    return 1;
}

static int readLine(FILE *file, char *line, size_t capacity) {
    if (!fgets(line, (int)capacity, file)) return 0;
    size_t length = strlen(line);
    if (!length || line[length - 1] != '\n') return 0;
    line[length - 1] = '\0';
    return 1;
}

static int readNumber(char **cursor, int lower, int upper, int *value) {
    char *end;
    errno = 0;
    long number = strtol(*cursor, &end, 10);
    if (errno || end == *cursor || number < lower || number > upper) return 0;
    *cursor = end;
    *value = (int)number;
    return 1;
}

static int readSeparator(char **cursor, char separator) {
    while (isspace((unsigned char)**cursor)) (*cursor)++;
    if (**cursor != separator) return 0;
    if (separator) (*cursor)++;
    return 1;
}

int mountFS(char *name) {
    char path[sizeof(partitionPath)];
    if (!makePath(path, name)) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;

    struct PARTITION disk;
    struct FAT entries[MAX_FILES];
    for (int i = 0; i < MAX_FILES; i++) entries[i] = emptyEntry();
    unsigned char *data = NULL;
    char *buffer = NULL;
    char line[MAX_NAME + 64];
    char *cursor = line;
    int ok = 0;
    int usedBlocks[MAX_FILES * MAX_BLOCKS];
    int usedCount = 0;

    if (!readLine(file, line, sizeof(line))) goto done;
    int versioned = strncmp(line, "MFS1 ", 5) == 0;
    if (versioned) cursor += 5;
    if (!readNumber(&cursor, 1, MAX_PARTITION_BYTES, &disk.block_size) ||
        !readSeparator(&cursor, ',') ||
        !readNumber(&cursor, 1, MAX_PARTITION_BYTES, &disk.total_blocks) ||
        !readSeparator(&cursor, ',') ||
        !readNumber(&cursor, 0, disk.total_blocks, &disk.last_free_block) ||
        !readSeparator(&cursor, '\0')) goto done;
    size_t size = diskSize(disk);
    if (!size) goto done;
    int maxFileLength = disk.block_size * MAX_BLOCKS;
    for (int i = 0; i < MAX_FILES; i++) {
        struct FAT *entry = &entries[i];
        if (!readLine(file, line, sizeof(line))) goto done;
        cursor = strchr(line, ',');
        if (!cursor) goto done;
        *cursor++ = '\0';
        if (!readNumber(&cursor, -1, maxFileLength, &entry->file_length) ||
            !readSeparator(&cursor, '\0')) goto done;
        if (strcmp(line, "(null)") == 0) {
            if (entry->file_length != -1) goto done;
        } else {
            if (!validName(line) || entry->file_length < 0) goto done;
            for (int j = 0; j < i; j++) {
                if (entries[j].filename && strcmp(entries[j].filename, line) == 0) goto done;
            }
            entry->filename = strdup(line);
            if (!entry->filename) goto done;
            entry->current_location = 0;
        }
        int needed = entry->file_length > 0 ? blocksForLength(entry->file_length, disk.block_size) : 0;
        /* The original block API stored a block count in this same field. */
        if (!versioned && disk.block_size > 1 && needed > 0 && entry->file_length == needed) goto done;
        if (!readLine(file, line, sizeof(line))) goto done;
        cursor = line;
        for (int j = 0; j < MAX_BLOCKS; j++) {
            if (j && !readSeparator(&cursor, ';')) goto done;
            if (!readNumber(&cursor, -1, disk.last_free_block - 1, &entry->blockPtrs[j])) goto done;
            int block = entry->blockPtrs[j];
            if ((j < needed) != (block >= 0)) goto done;
            if (block >= 0) {
                for (int k = 0; k < usedCount; k++) {
                    if (usedBlocks[k] == block) goto done;
                }
                usedBlocks[usedCount++] = block;
            }
        }
        if (!readSeparator(&cursor, '\0')) goto done;
    }
    if (usedCount != disk.last_free_block) goto done;
    data = malloc(size);
    buffer = malloc((size_t)disk.block_size + 1);
    if (!data || !buffer || fread(data, 1, size, file) != size ||
        fgetc(file) != EOF || ferror(file)) goto done;
    ok = 1;

done:
    if (fclose(file) != 0) ok = 0;
    if (ok) {
        installDisk(path, disk, entries, data, buffer);
        return 1;
    }
    for (int i = 0; i < MAX_FILES; i++) free(entries[i].filename);
    free(data);
    free(buffer);
    return 0;
}

int mountOrCreateFS(char *name, int blocksize, int totalblocks) {
    char path[sizeof(partitionPath)];
    if (!makePath(path, name)) return 0;
    struct stat info;
    if (stat(path, &info) == 0) return S_ISREG(info.st_mode) && mountFS(name);
    if (errno != ENOENT) return 0;
    return partition(name, blocksize, totalblocks);
}

int diskOpenEntry(const char *name) {
    if (!partitionData || !validName(name)) return -1;
    int freeEntry = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fat[i].filename) {
            if (strcmp(fat[i].filename, name) == 0) return i;
        } else if (freeEntry < 0) {
            freeEntry = i;
        }
    }
    if (freeEntry < 0) return -1;
    char *copy = strdup(name);
    if (!copy) return -1;
    struct FAT entries[MAX_FILES];
    memcpy(entries, fat, sizeof(entries));
    entries[freeEntry].filename = copy;
    entries[freeEntry].file_length = 0;
    entries[freeEntry].current_location = 0;
    if (!saveDisk(partitionPath, currentPartition, entries, partitionData)) {
        free(copy);
        return -1;
    }
    fat[freeEntry] = entries[freeEntry];
    return freeEntry;
}

static int validEntry(int file, int offset) {
    return partitionData && file >= 0 && file < MAX_FILES && fat[file].filename &&
        offset >= 0 && offset <= fat[file].file_length;
}

static size_t copyBytes(unsigned char *data, const struct FAT *entry, size_t offset, void *buffer,
                        size_t count, int writing) {
    size_t blocksize = (size_t)currentPartition.block_size;
    size_t copied = 0;
    while (copied < count) {
        size_t logical = offset / blocksize;
        size_t within = offset % blocksize;
        size_t chunk = blocksize - within;
        if (chunk > count - copied) chunk = count - copied;
        unsigned char *disk = data + (size_t)entry->blockPtrs[logical] * blocksize + within;
        unsigned char *bytes = (unsigned char *)buffer + copied;
        if (writing) memcpy(disk, bytes, chunk);
        else memcpy(bytes, disk, chunk);
        offset += chunk;
        copied += chunk;
    }
    return copied;
}

int diskReadBytes(int file, int offset, void *output, size_t count) {
    if (!validEntry(file, offset) || (!output && count)) return -1;
    size_t available = (size_t)(fat[file].file_length - offset);
    if (count > available) count = available;
    return (int)copyBytes(partitionData, &fat[file], (size_t)offset, output, count, 0);
}

int diskWriteBytes(int file, int offset, const void *data, size_t count) {
    if (!validEntry(file, offset) || (!data && count)) return -1;
    if (!count) return 0;
    struct PARTITION disk = currentPartition;
    int maxFileLength = disk.block_size * MAX_BLOCKS;
    if (count > (size_t)(maxFileLength - offset)) return -1;
    int end = offset + (int)count;
    int length = end > fat[file].file_length ? end : fat[file].file_length;
    int oldBlocks = blocksForLength(fat[file].file_length, disk.block_size);
    int requiredBlocks = blocksForLength(length, disk.block_size);
    if (requiredBlocks - oldBlocks > disk.total_blocks - disk.last_free_block) return -1;

    size_t size = diskSize(disk);
    unsigned char *nextData = malloc(size);
    if (!nextData) return -1;
    memcpy(nextData, partitionData, size);
    struct FAT entries[MAX_FILES];
    memcpy(entries, fat, sizeof(entries));
    for (int i = oldBlocks; i < requiredBlocks; i++) {
        entries[file].blockPtrs[i] = disk.last_free_block++;
    }
    entries[file].file_length = length;
    copyBytes(nextData, &entries[file], (size_t)offset, (void *)data, count, 1);
    if (!saveDisk(partitionPath, disk, entries, nextData)) {
        free(nextData);
        return -1;
    }
    free(partitionData);
    partitionData = nextData;
    currentPartition = disk;
    fat[file] = entries[file];
    return (int)count;
}

int inspectDiskBlock(int block, unsigned char *bytes, size_t capacity) {
    if (!partitionData || !bytes || block < 0 || block >= currentPartition.total_blocks ||
        capacity < (size_t)currentPartition.block_size) return -1;
    size_t blocksize = (size_t)currentPartition.block_size;
    memcpy(bytes, partitionData + (size_t)block * blocksize, blocksize);
    return currentPartition.block_size;
}

static int activeSlot(int file) {
    if (file < 0 || file >= MAX_FILES) return -1;
    for (int i = 0; i < MAX_OPENFILES; i++) {
        if (active_file_table[i] && active_file_table_owners[i] == file) return i;
    }
    return -1;
}

int openFile(char *name) {
    int entry = diskOpenEntry(name);
    if (entry < 0 || activeSlot(entry) >= 0) return entry;
    for (int i = 0; i < MAX_OPENFILES; i++) {
        if (active_file_table[i]) continue;
        active_file_table[i] = fopen(partitionPath, "rb");
        if (!active_file_table[i]) return -1;
        active_file_table_owners[i] = entry;
        fat[entry].current_location = 0;
        return entry;
    }
    return -1;
}

void closeFile(int file) {
    int slot = activeSlot(file);
    if (slot < 0) return;
    fclose(active_file_table[slot]);
    active_file_table[slot] = NULL;
    active_file_table_owners[slot] = -1;
}

char *readBlock(int file) {
    if (activeSlot(file) < 0 || fat[file].current_location >= MAX_BLOCKS) return NULL;
    int offset = fat[file].current_location * currentPartition.block_size;
    int length = diskReadBytes(file, offset, blockBuffer, (size_t)currentPartition.block_size);
    if (length <= 0) return NULL;
    blockBuffer[length] = '\0';
    fat[file].current_location++;
    return blockBuffer;
}

int writeBlock(int file, char *data) {
    if (activeSlot(file) < 0 || !data) return 0;
    size_t count = strnlen(data, (size_t)currentPartition.block_size);
    return diskWriteBytes(file, fat[file].file_length, data, count) >= 0;
}
