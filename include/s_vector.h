#ifndef S_VECTOR_H
#define S_VECTOR_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef struct s_vector
{
    char** data;
    size_t size;
    size_t capacity;
} s_vector;

void add_string(s_vector* lines, char* buffer, bool copy);
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread);
void erase(s_vector* vec, int pos);
void free_s_vector(s_vector* vector);

#endif
