#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "DISK_driver.h"
#include "DISK_driver_problem1.h"
#include "DISK_driver_problem2.h"
#include "DISK_driver_problem3.h"

enum {
    BLOCK_SIZE = 5,
    BLOCK_COUNT = 10,
    DISK_CAPACITY = BLOCK_SIZE * BLOCK_COUNT,
    DEMO_FD = 0,
    ERR_NOT_MOUNTED = -100,
    ERR_INVALID_INPUT = -101,
    ERR_DISK_FAILURE = -102,
};

static char disk_name[] = "browser-demo";
static char demo_file[] = "report.txt";
static bool mounted;
static char read_output[DISK_CAPACITY + 1];
static char snapshot[2048];

static void close_descriptor(void) {
    if (initialized && open_files[DEMO_FD].fat_index >= 0) {
        closeExamImpl(DEMO_FD);
    }
}

int mini_fs_format(void) {
    close_descriptor();
    mounted = false;
    read_output[0] = '\0';
    if (mkdir("PARTITION", 0700) != 0 && errno != EEXIST) {
        return ERR_DISK_FAILURE;
    }
    if (!partition(disk_name, BLOCK_SIZE, BLOCK_COUNT)) {
        return ERR_DISK_FAILURE;
    }
    mounted = mountFS(disk_name) != 0;
    return mounted ? 0 : ERR_DISK_FAILURE;
}

int mini_fs_remount(void) {
    if (!mounted) return ERR_NOT_MOUNTED;
    close_descriptor();
    read_output[0] = '\0';
    mounted = mountFS(disk_name) != 0;
    return mounted ? 0 : ERR_DISK_FAILURE;
}

int mini_fs_open(void) {
    if (!mounted) return ERR_NOT_MOUNTED;
    return openExamImpl(DEMO_FD, demo_file);
}

int mini_fs_close(void) {
    if (!mounted) return ERR_NOT_MOUNTED;
    return closeExamImpl(DEMO_FD);
}

int mini_fs_seek(int offset) {
    if (!mounted) return ERR_NOT_MOUNTED;
    return seekExamImpl(DEMO_FD, offset);
}

int mini_fs_write(char *text) {
    if (!mounted) return ERR_NOT_MOUNTED;
    if (text == NULL) return ERR_INVALID_INPUT;
    size_t length = strlen(text);
    if (length == 0 || length > DISK_CAPACITY) return ERR_INVALID_INPUT;
    for (size_t i = 0; i < length; i++) {
        if (text[i] < ' ' || text[i] > '~') return ERR_INVALID_INPUT;
    }
    return writeExamImpl(DEMO_FD, text);
}

int mini_fs_read(void) {
    if (!mounted) return ERR_NOT_MOUNTED;
    char *bytes = NULL;
    read_output[0] = '\0';
    int count = readExamImpl(DEMO_FD, &bytes);
    if (count > DISK_CAPACITY || (count > 0 && bytes == NULL)) {
        free(bytes);
        return ERR_DISK_FAILURE;
    }
    if (count > 0) {
        memcpy(read_output, bytes, (size_t)count);
        read_output[count] = '\0';
    }
    free(bytes);
    return count;
}

const char *mini_fs_read_output(void) {
    return read_output;
}

static void append_json(size_t *used, const char *format, ...) {
    if (*used >= sizeof snapshot) return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(snapshot + *used, sizeof snapshot - *used, format, args);
    va_end(args);
    *used = written < 0 ? sizeof snapshot : *used + (size_t)written;
}

const char *mini_fs_snapshot(void) {
    if (!mounted) {
        return "{\"mounted\":false,\"blockSize\":0,\"totalBlocks\":0,"
               "\"nextFreeBlock\":0,\"files\":[],\"blocks\":[],\"descriptor\":null}";
    }

    size_t used = 0;
    append_json(&used, "{\"mounted\":true,\"blockSize\":%d,\"totalBlocks\":%d,\"nextFreeBlock\":%d,\"files\":[",
                currentPartition.block_size, currentPartition.total_blocks, currentPartition.last_free_block);
    bool has_file = false;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fat[i].filename == NULL) continue;
        /* The bridge only accepts its fixed, JSON-safe file name. */
        append_json(&used, "%s{\"name\":\"%s\",\"length\":%d,\"blocks\":[",
                    has_file ? "," : "", fat[i].filename, fat[i].file_length);
        bool has_block = false;
        for (int j = 0; j < MAX_BLOCKS; j++) {
            if (fat[i].blockPtrs[j] < 0) continue;
            append_json(&used, "%s%d", has_block ? "," : "", fat[i].blockPtrs[j]);
            has_block = true;
        }
        append_json(&used, "]}");
        has_file = true;
    }

    append_json(&used, "],\"blocks\":[");
    for (int i = 0; i < currentPartition.total_blocks; i++) {
        unsigned char bytes[BLOCK_SIZE];
        int count = inspectDiskBlock(i, bytes, sizeof bytes);
        if (count != BLOCK_SIZE) return NULL;
        append_json(&used, "%s{\"index\":%d,\"bytes\":[", i ? "," : "", i);
        for (int j = 0; j < count; j++) {
            append_json(&used, "%s%u", j ? "," : "", (unsigned int)bytes[j]);
        }
        append_json(&used, "]}");
    }

    append_json(&used, "],\"descriptor\":");
    int file_index = initialized ? open_files[DEMO_FD].fat_index : -1;
    if (file_index >= 0 && file_index < MAX_FILES && fat[file_index].filename != NULL) {
        append_json(&used, "{\"fd\":%d,\"cursor\":%d,\"file\":\"%s\"}",
                    DEMO_FD, open_files[DEMO_FD].offset, fat[file_index].filename);
    } else {
        append_json(&used, "null");
    }
    append_json(&used, "}");
    return used < sizeof snapshot ? snapshot : NULL;
}
