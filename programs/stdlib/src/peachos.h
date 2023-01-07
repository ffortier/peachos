#ifndef PEACHOS_H
#define PEACHOS_H

#include <stddef.h>
#include <stdbool.h>

void print(const char *filename);
char peachos_getkey();
void *peachos_malloc(size_t size);
void peachos_putchar(char c);
void peachos_free(void *ptr);
int peachos_getkeyblock();
void peachos_terminal_readline(char *out, int max, bool output_while_typing);

#endif