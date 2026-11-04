#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DISK_driver.h"
#include "DISK_driver_problem1.h"
#include "DISK_driver_problem2.h"
#include "DISK_driver_problem3.h"
#include "DISK_driver_problem4.h"
#include "shellmemory.h"

static void expectRead(int fd, const char *expected) {
    char *value = NULL;
    assert(readExamImpl(fd, &value) == (int)strlen(expected));
    assert(value && strcmp(value, expected) == 0);
    free(value);
}

static void makeLegacyCopy(const char *source, const char *destination) {
    FILE *input = fopen(source, "rb");
    FILE *output = fopen(destination, "wb");
    assert(input && output);
    assert(fseek(input, 5, SEEK_SET) == 0); /* Strip the MFS1 marker. */
    int byte;
    while ((byte = fgetc(input)) != EOF) {
        assert(fputc(byte == 0 ? '0' : byte, output) != EOF);
    }
    assert(!ferror(input));
    assert(fclose(input) == 0);
    assert(fclose(output) == 0);
}

static void testDemo(void) {
    assert(partition("lab", 5, 10));
    assert(openExamImpl(0, "report.txt") == 0);
    assert(openExamImpl(0, "report.txt") == ERR_FD_OPEN);
    assert(writeExamImpl(0, "hello world test") == 16);
    assert(closeExamImpl(0) == 0);
    assert(openExamImpl(0, "report.txt") == 0);
    assert(readpipeExamImpl(0) == 0);
    char *piped = acceptPipeInput(0);
    assert(piped && setVariable("x", piped) == 0);
    free(piped);
    assert(seekExamImpl(0, -4) == 0);
    assert(readpipeExamImpl(0) == 0);
    piped = acceptPipeInput(0);
    assert(piped && strcmp(piped, "test") == 0);
    free(piped);
    expectRead(0, "");
    assert(readpipeExamImpl(0) == 0);
    piped = acceptPipeInput(0);
    assert(piped && !*piped);
    free(piped);
    assert(closeExamImpl(0) == 0);
    assert(acceptPipeInput(0) == NULL);
    assert(strcmp(readVariable("x"), "hello world test") == 0);
    assert(readVariable("missing") == NULL);
}

static void testPersistenceAndOverwrite(void) {
    assert(openExamImpl(0, "digits") == 0);
    assert(writeExamImpl(0, "A0CDEFGHIJKLMN") == 14);
    assert(seekExamImpl(0, 5) == 0);
    assert(writeExamImpl(0, "xy") == 2);
    assert(seekExamImpl(0, 0) == 0);
    expectRead(0, "A0CDExyHIJKLMN");
    assert(seekExamImpl(0, 4) == 0);
    assert(writeExamImpl(0, "uvw") == 3);
    assert(seekExamImpl(0, 0) == 0);
    expectRead(0, "A0CDuvwHIJKLMN");

    unsigned char block[5];
    assert(inspectDiskBlock(fat[examFileIndex(0)].blockPtrs[0], block, sizeof(block)) == 5);
    assert(memcmp(block, "A0CDu", 5) == 0);
    assert(inspectDiskBlock(-1, block, sizeof(block)) < 0);
    assert(inspectDiskBlock(10, block, sizeof(block)) < 0);
    assert(inspectDiskBlock(0, block, 4) < 0);

    assert(rename("PARTITION", "PARTITION.saved") == 0);
    assert(seekExamImpl(0, 0) == 0);
    assert(writeExamImpl(0, "BAD") < 0);
    assert(open_files[0].offset == 0);
    assert(rename("PARTITION.saved", "PARTITION") == 0);
    expectRead(0, "A0CDuvwHIJKLMN");

    FILE *broken = fopen("PARTITION/broken.txt", "wb");
    assert(broken && fputs("MFS1 5, 10, 0\ntruncated", broken) >= 0);
    assert(fclose(broken) == 0);
    assert(mountOrCreateFS("broken", 5, 10) == 0);
    assert(seekExamImpl(0, INT_MIN) == ERR_OUT_OF_LOWER_BOUNDS);
    expectRead(0, "A0CDuvwHIJKLMN");
    assert(seekExamImpl(0, INT_MAX) == ERR_OUT_OF_UPPER_BOUNDS);
    expectRead(0, "");

    makeLegacyCopy("PARTITION/lab.txt", "PARTITION/legacy.txt");
    assert(mountFS("legacy"));
    assert(readExamImpl(0, NULL) < 0);
    assert(openExamImpl(0, "digits") == 0);
    expectRead(0, "A0CDuvwHIJKLMN");
    assert(mountFS("lab"));
    assert(closeExamImpl(0) == ERR_FD_NOT_OPEN);
    assert(openExamImpl(0, "report.txt") == 0);
    expectRead(0, "hello world test");
}

static void testCapacityAndLegacyCalls(void) {
    assert(partition("bounds", 5, 12));
    assert(openExamImpl(0, "full") == 0);
    char full[] = "01234567890123456789012345678901234567890123456789";
    assert(writeExamImpl(0, full) == 50);
    assert(seekExamImpl(0, 50) == 0);
    assert(writeExamImpl(0, "z") < 0);
    assert(open_files[0].offset == 50);
    assert(currentPartition.last_free_block == 10);
    int other = openFile("legacy");
    assert(other >= 0);
    assert(writeBlock(other, "AB0CD") == 1);
    assert(writeBlock(other, "1234") == 1);
    assert(writeBlock(other, "xx") == 0);
    closeFile(other);
    for (int i = 0; i < MAX_OPENFILES + 2; i++) {
        int fd = openFile("legacy");
        assert(fd == other);
        assert(strcmp(readBlock(fd), "AB0CD") == 0);
        assert(strcmp(readBlock(fd), "1234") == 0);
        assert(readBlock(fd) == NULL);
        closeFile(fd);
    }
    assert(mountFS("bounds"));
    assert(openExamImpl(0, "full") == 0);
    expectRead(0, full);
    assert(openExamImpl(MAX_OPENFILES - 1, "legacy") == 0);
    expectRead(MAX_OPENFILES - 1, "AB0CD1234");
    assert(openExamImpl(MAX_OPENFILES, "legacy") == ERR_FD_NOT_OPEN);
    assert(openExamImpl(-1, "legacy") == ERR_FD_NOT_OPEN);
    assert(seekExamImpl(MAX_OPENFILES, 0) == ERR_FD_NOT_OPEN);
    assert(readpipeExamImpl(INT_MAX) == ERR_FD_NOT_OPEN);
    assert(acceptPipeInput(INT_MIN) == NULL);

    assert(partition("fullfat", 5, 1));
    for (int i = 0; i < MAX_FILES; i++) {
        char name[24];
        snprintf(name, sizeof(name), "file-%d", i);
        assert(openExamImpl(0, name) == 0);
        assert(closeExamImpl(0) == 0);
    }
    assert(openExamImpl(0, "overflow") == ERR_NO_SPACE);
    assert(openExamImpl(0, "file-19") == 0);
    expectRead(0, "");

    assert(partition("versioned-one-byte", 5, 1));
    assert(openExamImpl(0, "small") == 0);
    assert(writeExamImpl(0, "0") == 1);
    assert(mountFS("versioned-one-byte"));
    assert(openExamImpl(0, "small") == 0);
    expectRead(0, "0");
    makeLegacyCopy("PARTITION/versioned-one-byte.txt", "PARTITION/ambiguous.txt");
    assert(!mountFS("ambiguous"));
    assert(seekExamImpl(0, 0) == 0);
    expectRead(0, "0");
}

int main(void) {
    initIO();
    assert(openExamImpl(0, "unmounted") == ERR_NO_SPACE);
    assert(!partition("../escape", 5, 10));
    assert(!partition("invalid", 0, 10));
    assert(!partition("overflow", INT_MAX, INT_MAX));
    assert(openFile("unmounted") < 0);

    testDemo();
    testPersistenceAndOverwrite();
    testCapacityAndLegacyCalls();
    initIO();
    initializeShellMemory();
    puts("Mini File System driver regressions passed.");
    return 0;
}
