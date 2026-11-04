#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpreter.h"
#include "shell.h"

void freeParsedCommand(char *parsedCommand[]) {
    for (int i = 0; i < MAX_TOKENS; i++) {
        free(parsedCommand[i]);
        parsedCommand[i] = NULL;
    }
}

void parser(char *command, char *parsedCommand[]) {
    for (int i = 0; i < MAX_TOKENS; i++) parsedCommand[i] = NULL;
    const char *cursor = command;
    for (int i = 0; i < MAX_TOKENS - 1; i++) {
        while (isspace((unsigned char)*cursor)) cursor++;
        if (!*cursor || *cursor == '#') return;

        int bracketed = *cursor == '[';
        if (bracketed) cursor++;
        const char *start = cursor;
        if (bracketed) {
            while (*cursor && *cursor != ']' && *cursor != '\n' && *cursor != '\r') cursor++;
            if (*cursor != ']') {
                freeParsedCommand(parsedCommand);
                return;
            }
        } else {
            while (*cursor && !isspace((unsigned char)*cursor)) cursor++;
        }
        size_t size = (size_t)(cursor - start);
        parsedCommand[i] = malloc(size + 1);
        if (!parsedCommand[i]) {
            freeParsedCommand(parsedCommand);
            return;
        }
        memcpy(parsedCommand[i], start, size);
        parsedCommand[i][size] = '\0';
        if (bracketed) cursor++;
    }
}

int shellUI(void) {
    printf("Welcome to Mini File System!\n");
    for (;;) {
        char command[MAX_BUFFER];
        char *parsedCommand[MAX_TOKENS];

        printf("\n$ ");
        if (!fgets(command, sizeof(command), stdin)) break;
        if (!strchr(command, '\n') && !feof(stdin)) {
            int next;
            while ((next = fgetc(stdin)) != EOF && next != '\n') {}
            puts("Command is too long.");
            continue;
        }

        parser(command, parsedCommand);
        int returnCode = interpreter(parsedCommand);
        freeParsedCommand(parsedCommand);
        if (returnCode == 1)
            puts("Unknown command: Type \"help\" to find a list of valid commands with their syntax.");
    }
    return 0;
}
