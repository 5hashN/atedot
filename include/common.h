#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <locale.h>

static inline void* xcalloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (!p && count > 0 && size > 0) {
        fprintf(stderr, "Fatal: Out of memory\n");
        exit(1);
    }
    return p;
}

static inline void* xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p && size > 0) {
        fprintf(stderr, "Fatal: Out of memory\n");
        exit(1);
    }
    return p;
}

#define CALLOC(T, n)   ((T*)xcalloc((size_t)(n), sizeof(T)))
#define MALLOC(T, n)   ((T*)xmalloc((size_t)(n) * sizeof(T)))
#define REALLOC(T, p, n) ((T*)realloc((p), (size_t)(n) * sizeof(T)))