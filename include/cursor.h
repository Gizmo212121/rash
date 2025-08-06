#ifndef CURSOR_H
#define CURSOR_H

#define UNUSED(x) (void)(x)

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <assert.h>

typedef struct cursor_pos
{
    int x;
    int y;
} cursor_pos;

void move_cursor(int y, int x);
cursor_pos get_cursor();
void init_term_settings();
void set_term_echo_and_canonical(bool enable);
void recalculate_window_dimensions(int signum);
void initscr();
int win_size_x(void);
int win_size_y(void);

#endif
