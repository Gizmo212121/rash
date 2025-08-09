#include "../include/line.h"

size_t min_size = 8;

void initialize_line(line* l)
{
    assert(l != NULL);

    if (l->data)
    {
        fprintf(stderr, "Line already initialized\n");
        exit(EXIT_FAILURE);
    }

    l->data = malloc(min_size * sizeof(*l->data));
    if (!l->data)
    {
        perror("Line malloc\n");
        exit(EXIT_FAILURE);
    }

    *(l->data) = '\0';
    l->size = 0;
    l->capacity = min_size;
    l->cursor_pos = 0;
}

void initialize_line_with_new_data(line* l, char* d)
{
    char* copy = strdup(d);
    if (!copy)
    {
        perror("Line history strdup\n");
        exit(EXIT_FAILURE);
    }

    l->data = copy;

    int line_size = strlen(l->data);
    l->size = line_size;
    l->cursor_pos = line_size;
    l->capacity = line_size + 1;
}

// Doubles a line's capacity and reallocs the data
void increase_line_capacity(line * l)
{
    assert(l != NULL);

    l->capacity *= 2;
    l->data = realloc(l->data, l->capacity * sizeof(*l->data));
    if (!l->data)
    {
        perror("Line realloc\n");
        exit(EXIT_FAILURE);
    }
}


void push_back_character(line* l, char c)
{
    assert(l != NULL);

    if (!l->data) { initialize_line(l); }

    if (l->size + 1 == l->capacity) { increase_line_capacity(l); }

    *(l->data + l->cursor_pos) = c;
    *(l->data + l->cursor_pos + 1) = '\0';

    l->size++;
    l->cursor_pos++;
}

// Inserts a character c into line l at wherever the current cursor position is. Resizes if needed
void insert_character(line* l, char c)
{
    assert(l != NULL);

    if (!l->data) { initialize_line(l); }

    if (l->size + 1 == l->capacity) { increase_line_capacity(l); }

    if (l->size != l->cursor_pos)
    {
        memmove(l->data + l->cursor_pos + 1, l->data + l->cursor_pos, l->size - l->cursor_pos);
        *(l->data + l->cursor_pos) = c;
    }
    else
    {
        *(l->data + l->cursor_pos) = c;
        *(l->data + l->cursor_pos + 1) = '\0';
    }

    l->size++;
    l->cursor_pos++;
}

bool remove_character(line* l)
{
    assert(l != NULL);

    if (!l->data) { initialize_line(l); }

    if (!l->size) { return false; }
    if (!l->cursor_pos) { return false; }

    if (l->size == l->cursor_pos)
    {
        *(l->data + l->size - 1) = '\0';
    }
    else
    {
        memmove(l->data + l->cursor_pos - 1, l->data + l->cursor_pos, l->size + 1 - l->cursor_pos);
    }

    l->size--;
    l->cursor_pos--;

    return true;
}

void clear_line(line* l)
{
    assert(l != NULL);

    l->size = 0;
    l->cursor_pos = 0;
}

void clear_line_and_free(line* l)
{
    clear_line(l);
    free(l->data);
    l->data = NULL;
}

void move_line_cursor_x(line* l, int x)
{
    assert(l != NULL);

    int final_position = (int)l->cursor_pos + x;
    if (final_position >= 0 && final_position <= (int)l->size)
    {
        l->cursor_pos = (size_t)final_position;
    }
}
