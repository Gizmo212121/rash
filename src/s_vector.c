#include "../include/s_vector.h"

// Add string to dynamic array of strings
// Capacity doubles when full and attempting to add string
// If copy is true, the original buffer is copied. If copy is false, the pointer is copied
void add_string(s_vector* lines, char* buffer, bool copy)
{
    // if (buffer && !*buffer) { return; } // TODO: Could be bad

    if (lines->size == lines->capacity)
    {
        lines->capacity = (lines->capacity == 0) ? 1 : lines->capacity << 1;
        char** temp = realloc(lines->data, sizeof(*lines->data) * lines->capacity);
        if (!temp)
        {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        lines->data = temp;
    }

    if (copy)
        lines->data[lines->size] = buffer ? strdup(buffer) : NULL;
    else
        lines->data[lines->size] = buffer;

    lines->size++;
}

// add_string but works on input buffer of variable length where actual string size might not match
void buf_add_string(s_vector* lines, char* buffer, ssize_t nread)
{
    if (buffer && !*buffer) { return; }

    if (lines->size == lines->capacity)
    {
        lines->capacity = (lines->capacity == 0) ? 1 : lines->capacity << 1;
        char** temp = realloc(lines->data, sizeof(*lines->data) * lines->capacity);
        if (!temp)
        {
            // TODO: Free on exit?
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        lines->data = temp;
    }

    lines->data[lines->size] = buffer ? strndup(buffer, nread) : NULL;
    lines->size++;
}

void erase(s_vector* vec, int pos)
{
    if (pos < 0 || pos >= (int)vec->size) { fprintf(stderr, "erase: invalid range\n"); exit(EXIT_FAILURE); }

    for (int i = pos; i < (int)vec->size; i++)
    {
        free(vec->data[i]);
        vec->data[i] = NULL;
    }

    vec->size = pos;
}

// free's memory of s_vector
void free_s_vector(s_vector* vector)
{
    for (size_t i = 0; i < vector->size; i++)
    {
        free(vector->data[i]);
    }
    free(vector->data);
}
