#ifndef _READ_TRACE_H_
#define _READ_TRACE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "read_trace.h"

#define MAX_LINE_LENGTH 256

char **read_trace(const char *filename, int rows) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening file");
        return NULL;
    }

    char **matrix = malloc(rows * sizeof(char *));
    if (!matrix) {
        perror("malloc failed");
        fclose(fp);
        return NULL;
    }

    // Initialize rows to NULL
    for (int i = 0; i < rows; i++) {
        matrix[i] = NULL;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), fp)) {
        char *tab_pos = strchr(line, '\t');
        if (!tab_pos) {
            fprintf(stderr, "Invalid line format (missing tab): %s", line);
            continue;
        }

        *tab_pos = '\0'; // terminate number part
        int row_index = atoi(line);
        if (row_index < 0 || row_index >= rows) {
            fprintf(stderr, "Invalid row index: %d\n", row_index);
            continue;
        }

        char *sequence = tab_pos + 1;
        sequence[strcspn(sequence, "\n")] = '\0'; // remove trailing newline

        matrix[row_index] = malloc(strlen(sequence) + 1);
        if (!matrix[row_index]) {
            perror("malloc failed for row content");
            fclose(fp);
            for (int i = 0; i < rows; i++) free(matrix[i]);
            free(matrix);
            return NULL;
        }
        strcpy(matrix[row_index], sequence);
    }
    fclose(fp);
    return matrix;
}
#endif