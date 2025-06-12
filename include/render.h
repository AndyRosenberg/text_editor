#ifndef RENDER
#define RENDER

#include "types.h"

void editor_draw_rows(append_buffer *ab);
void editor_draw_status_bar(append_buffer *ab);
void editor_draw_message_bar(append_buffer *ab);
int editor_row_cursor_x_to_render_x(editor_row *row, int cursor_x);
int editor_row_render_x_to_cursor_x(editor_row *row, int render_x);
char *editor_rows_to_string(int *buffer_length);
void editor_scroll(void);
void editor_refresh_screen(void);
void editor_set_status_message(const char *fmt, ...);
int get_window_size(int *rows, int *cols);
char *editor_prompt(char *prompt, void(*callback)(char *, int));

#endif
