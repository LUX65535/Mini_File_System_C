#include <stdlib.h>
#include <string.h>

#include "shellmemory.h"

struct MEM {
    char *var;
    char *value;
};

static struct MEM variables[MAX_VARS];

int initializeShellMemory(void) {
    for (int i = 0; i < MAX_VARS; i++) {
        free(variables[i].var);
        free(variables[i].value);
        variables[i] = (struct MEM){0};
    }
    return 0;
}

static char *copyString(const char *value) {
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, value, size);
    return copy;
}

int setVariable(char var[], char value[]) {
    if (!var || !*var || !value) return 5;
    int i;
    for (i = 0; i < MAX_VARS; i++) {
        if (!variables[i].var || strcmp(variables[i].var, var) == 0) break;
    }
    if (i == MAX_VARS) return 5;

    char *copy = copyString(value);
    if (!copy) return 5;
    if (!variables[i].var) {
        variables[i].var = copyString(var);
        if (!variables[i].var) {
            free(copy);
            return 5;
        }
    }
    free(variables[i].value);
    variables[i].value = copy;
    return 0;
}

char *readVariable(char var[]) {
    if (!var) return NULL;
    for (int i = 0; i < MAX_VARS && variables[i].var; i++) {
        if (strcmp(variables[i].var, var) == 0) return variables[i].value;
    }
    return NULL;
}
