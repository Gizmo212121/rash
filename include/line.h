#ifndef LINE_H
#define LINE_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

extern size_t min_size;

typedef struct line
{
    char* data;
    size_t size;
    size_t capacity;
    size_t cursor_pos;
} line;

void initialize_line(line* l);
void increase_line_capacity(line * l);
void insert_character(line* l, char c);
void push_back_character(line* l, char c);
bool remove_character(line* l);
void clear_line(line* l);
void move_line_cursor_x(line* l, int x);

#endif
