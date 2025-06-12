#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../include/constants.h"
#include "../include/types.h"
#include "../include/utils.h"
#include "../include/write.h"
#include "../include/navigate.h"
#include "../include/syntax.h"

void editor_draw_rows(append_buffer *ab) {
  int y;
  for (y = 0; y < edconfig.screen_rows; y++) {
    int file_row = y + edconfig.row_offset;
    if (file_row >= edconfig.number_of_rows) {
      // check for blank file
      if (!edconfig.number_of_rows &&
            y == edconfig.screen_rows / 3) {
        char welcome[80];

        int welcome_length = snprintf(
          welcome,
          sizeof(welcome),
          "Koji Editor -- Version %s",
          KOJI_VERSION
        );

        if (welcome_length > edconfig.screen_columns) {
          welcome_length = edconfig.screen_columns;
        }

        int padding = (edconfig.screen_columns - welcome_length) / 2;

        if (padding) {
          ab_append(ab, "~", 1);
          padding--;
        }

        while (padding--) {
          ab_append(ab, " ", 1);
        }

        ab_append(ab, welcome, welcome_length);
      } else {
        ab_append(ab, "~", 1);
      }
    } else {
      // read file contents up to current row
      int last_row_length = edconfig.current_rows[
        file_row
      ].render_size - edconfig.column_offset;

      if (last_row_length < 0) {
        last_row_length = 0;
      }

      if (last_row_length > edconfig.screen_columns) {
        last_row_length = edconfig.screen_columns;
      }

      char *c = &edconfig.current_rows[
        file_row
      ].render[edconfig.column_offset];

      unsigned char *highlight = &edconfig.current_rows[
        file_row
      ].highlight[edconfig.column_offset];

      int current_color = -1;
      int j;

      for (j = 0; j < last_row_length; j++) {
        if (iscntrl(c[j])) {
          char symbol = (c[j] <= 26) ? '@' + c[j] : '?';
          ab_append(ab, "\x1b[7m", 4);
          ab_append(ab, &symbol, 1);
          ab_append(ab, "\x1b[m", 3);

          if (current_color != -1) {
            char buffer[16];
            int color_length = snprintf(
              buffer,
              sizeof(buffer),
              "\x1b[%dm",
              current_color
            );
            ab_append(ab, buffer, color_length);
          }
        } else if (highlight[j] == HIGHLIGHT_NORMAL) {
          if (current_color != -1) {
            ab_append(ab, "\x1b[39m", 5);
            current_color = -1;
          }

          ab_append(ab, &c[j], 1);
        } else {
          int color = editor_syntax_to_color(highlight[j]);
          if (color != current_color) {
            current_color = color;
            char buffer[16];
            int color_length = snprintf(
              buffer,
              sizeof(buffer),
              "\x1b[%dm",
              color
            );
            ab_append(ab, buffer, color_length);
          }

          ab_append(ab, &c[j], 1);
        }
      }
      // set back to normal color
      ab_append(ab, "\x1b[39m", 5);
    }

    // append newlines and clear other terminal contents
    ab_append(ab, "\x1b[K", 3);
    ab_append(ab, "\r\n", 2);
  }
}

void editor_draw_status_bar(append_buffer *ab) {
  ab_append(ab, "\x1b[7m", 4);

  char status_bar_left_text[80];
  char status_bar_right_text[80];

  int status_bar_left_len = snprintf(
    status_bar_left_text,
    sizeof(status_bar_left_text),
    "%.20s - %d lines %s",
    edconfig.file_name ? edconfig.file_name : "[No Name]",
    edconfig.number_of_rows,
    edconfig.is_dirty ? "(modified)": ""
  );

  int status_bar_right_len = snprintf(
    status_bar_right_text,
    sizeof(status_bar_right_text),
    "filetype - %s | line %d/%d",
    edconfig.syntax ? edconfig.syntax->file_type : "no filetype",
    edconfig.cursor_y + 1,
    edconfig.number_of_rows
  );

  if (status_bar_left_len > edconfig.screen_columns) {
    status_bar_left_len = edconfig.screen_columns;
  }

  ab_append(ab, status_bar_left_text, status_bar_left_len);

  while (status_bar_left_len < edconfig.screen_columns) {
    if (edconfig.screen_columns - status_bar_left_len == status_bar_right_len) {
      ab_append(ab, status_bar_right_text, status_bar_right_len);
      break;
    } else {
      ab_append(ab, " ", 1);
      status_bar_left_len++;
    }
  }

  ab_append(ab, "\x1b[m", 3);
  ab_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(append_buffer *ab) {
  ab_append(ab, "\x1b[K", 3);
  int message_length = strlen(edconfig.status_message);

  if (message_length > edconfig.screen_columns) {
    message_length = edconfig.screen_columns;
  }

  if (message_length && time(NULL) - edconfig.status_message_time < 5) {
    ab_append(ab, edconfig.status_message, message_length);
  }
}

int editor_row_cursor_x_to_render_x(editor_row *row, int cursor_x) {
  int render_x = 0;
  int j;

  for (j = 0; j < cursor_x; j++) {
    if (row->chars[j] == '\t') {
      render_x += (KOJI_TAB_STOP - 1) - (render_x % KOJI_TAB_STOP);
    }

    render_x++;
  }

  return render_x;
}

int editor_row_render_x_to_cursor_x(editor_row *row, int render_x) {
  int current_render_x = 0;
  int cursor_x;

  for (cursor_x = 0; cursor_x < row->size; cursor_x++) {
    if (row->chars[cursor_x] == '\t') {
      current_render_x += (KOJI_TAB_STOP - 1) - (
        current_render_x % KOJI_TAB_STOP
      );
    }

    current_render_x++;

    if (current_render_x > render_x) {
      return cursor_x;
    }
  }

  return cursor_x;
}

char *editor_rows_to_string(int *buffer_length) {
  int total_length = 0;
  int j;

  for (j = 0; j < edconfig.number_of_rows; j++) {
    total_length += edconfig.current_rows[j].size + 1;
  }

  *buffer_length = total_length;

  char *buffer = malloc(total_length);
  char *pointer = buffer;

  for (j = 0; j < edconfig.number_of_rows; j++) {
    memcpy(
      pointer,
      edconfig.current_rows[j].chars,
      edconfig.current_rows[j].size
    );

    pointer += edconfig.current_rows[j].size;
    *pointer = '\n';
    pointer++;
  }

  return buffer;
}

void editor_scroll(void) {
  edconfig.render_x = 0;

  if (edconfig.cursor_y < edconfig.number_of_rows) {
    edconfig.render_x = editor_row_cursor_x_to_render_x(
      &edconfig.current_rows[edconfig.cursor_y],
      edconfig.cursor_x
    );
  }

  if (edconfig.cursor_y < edconfig.row_offset) {
    edconfig.row_offset = edconfig.cursor_y;
  }

  if (edconfig.cursor_y >= edconfig.row_offset + edconfig.screen_rows) {
    edconfig.row_offset = edconfig.cursor_y - edconfig.screen_rows + 1;
  }

  if (edconfig.render_x < edconfig.column_offset) {
    edconfig.column_offset = edconfig.render_x;
  }

  if (edconfig.render_x >= edconfig.column_offset + edconfig.screen_columns) {
    edconfig.column_offset = edconfig.render_x - edconfig.screen_columns + 1;
  }
}

void editor_refresh_screen(void) {
  editor_scroll();

  append_buffer ab = APPEND_BUFFER_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);
  editor_draw_status_bar(&ab);
  editor_draw_message_bar(&ab);

  char cursor_buffer[32];

  snprintf(
    cursor_buffer,
    sizeof(cursor_buffer),
    "\x1b[%d;%dH",
    (edconfig.cursor_y - edconfig.row_offset) + 1,
    (edconfig.render_x - edconfig.column_offset) + 1
  );

  ab_append(&ab, cursor_buffer, strlen(cursor_buffer));
  ab_append(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.buffer, ab.len);
  ab_free(&ab);
}

void editor_set_status_message(const char *fmt, ...) {
  va_list params;
  va_start(params, fmt);
  vsnprintf(
    edconfig.status_message,
    sizeof(edconfig.status_message),
    fmt,
    params
  );
  va_end(params);
  edconfig.status_message_time = time(NULL);
}

int get_window_size(int *rows, int *cols) {
  struct winsize window_size;
  
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1 || window_size.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return -1;
    }

    return get_cursor_position(rows, cols);
  } else {
    *cols = window_size.ws_col;
    *rows = window_size.ws_row;
    return 0;
  }
}

char *editor_prompt(char *prompt, void(*callback)(char *, int)) {
  size_t buffer_size = 128;
  char *buffer = malloc(buffer_size);
  size_t buffer_length = 0;

  buffer[0] = '\0';

  while (1) {
    editor_set_status_message(prompt, buffer);
    editor_refresh_screen();

    int c = editor_read_key();

    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buffer_length != 0) {
        buffer[--buffer_length] = '\0';
      }
    } else if (c == '\x1b') {
      editor_set_status_message("");

      if (callback) {
        callback(buffer, c);
      }
      free(buffer);
      return NULL;
    } else if (c == '\r') {
      if (buffer_length != 0) {
        editor_set_status_message("");

        if (callback) {
          callback(buffer, c);
        }

        return buffer;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buffer_length == buffer_size - 1) {
        buffer_size *= 2;
        buffer = realloc(buffer, buffer_size);
      }
      buffer[buffer_length++] = c;
      buffer[buffer_length] = '\0';
    }

    if (callback) {
      callback(buffer, c);
    }
  }
}
