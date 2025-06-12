#ifndef UTILS
#define UTILS

#include "types.h"

void ab_append(append_buffer *ab, const char *s, int len);
void ab_free(append_buffer *ab);
void editor_clear_screen(void);
void die(const char *s);
void disable_raw_mode(void);
void enable_raw_mode(void);

#endif
