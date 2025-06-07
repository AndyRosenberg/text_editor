#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define KOJI_VERSION "0.0.1"
#define KOJI_TAB_STOP 8
#define KOJI_QUIT_TIMES 1
#define CTRL_KEY(k) ((k) & 0x1f)
#define APPEND_BUFFER_INIT { NULL, 0 }

enum MOVEMENT_KEYS {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

enum EDITOR_HIGHLIGHT {
  HIGHLIGHT_NORMAL = 0,
  HIGHLIGHT_NUMBER,
  HIGHLIGHT_MATCH
};

typedef struct {
  char *buffer;
  int len;
} append_buffer;

typedef struct {
  int size;
  int render_size;
  char *chars;
  char *render;
  unsigned char *highlight;
} editor_row;

typedef struct {
  int cursor_x;
  int cursor_y;
  int render_x;
  int row_offset;
  int column_offset;
  int screen_rows;
  int screen_columns;
  int number_of_rows;
  editor_row *current_rows;
  int is_dirty;
  char *file_name;
  char status_message[80];
  time_t status_message_time;
  struct termios orig_termios;
} editor_cofig;

editor_cofig edconfig;

/*** Prototypes ***/
char *editor_prompt(char *prompt, void(*callback)(char *, int));
int editor_syntax_to_color(int highlight);
void editor_update_syntax(editor_row *row);
/******/

void ab_append(append_buffer *ab, const char *s, int len) {
  char *new = realloc(ab->buffer, ab->len + len);

  if (new == NULL) {
    return;
  }

  memcpy(&new[ab->len], s, len);
  ab->buffer = new;
  ab->len += len;
}

void ab_free(append_buffer *ab) {
  free(ab->buffer);
}

void editor_clear_screen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void die(const char *s) {
  editor_clear_screen();
  perror(s);
  exit(1);
}

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

      unsigned char *hl = &edconfig.current_rows[
        file_row
      ].highlight[edconfig.column_offset];

      int current_color = -1;
      int j;

      for (j = 0; j < last_row_length; j++) {
        if (hl[j] == HIGHLIGHT_NORMAL) {
          if (current_color != -1) {
            ab_append(ab, "\x1b[39m", 5);
            current_color = -1;
          }

          ab_append(ab, &c[j], 1);
        } else {
          int color = editor_syntax_to_color(hl[j]);
          if (color != current_color) {
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

        // switch back to normal color
        ab_append(ab, "\x1b[39m", 5);
      }
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
    "line %d/%d",
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

void editor_update_row(editor_row *row) {
  int tabs = 0;
  int j;

  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      tabs++;
    }
  }

  free(row->render);
  row->render = malloc(
    row->size + tabs * (KOJI_TAB_STOP - 1) + 1
  );

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';

      while (idx % KOJI_TAB_STOP != 0) {
        row->render[idx++] = ' ';
      }
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  row->render_size = idx;

  editor_update_syntax(row);
}

void editor_insert_row(int idx, char *s, size_t len) {
  if (idx < 0 || idx > edconfig.number_of_rows) {
    return;
  }

  edconfig.current_rows = realloc(
    edconfig.current_rows,
    sizeof(editor_row) * (edconfig.number_of_rows + 1)
  );

  memmove(
    &edconfig.current_rows[idx + 1],
    &edconfig.current_rows[idx],
    sizeof(editor_row) * (edconfig.number_of_rows - idx)
  );

  edconfig.current_rows[idx].size = len;
  edconfig.current_rows[idx].chars = malloc(len + 1);

  memcpy(edconfig.current_rows[idx].chars, s, len);

  edconfig.current_rows[idx].chars[len] = '\0';

  edconfig.current_rows[idx].render_size = 0;
  edconfig.current_rows[idx].render = NULL;
  edconfig.current_rows[idx].highlight = NULL;
  editor_update_row(&edconfig.current_rows[idx]);

  edconfig.number_of_rows++;
  edconfig.is_dirty++;
}

void editor_free_row(editor_row *row) {
  free(row->render);
  free(row->chars);
  free(row->highlight);
}

void editor_delete_row(int idx) {
  if (idx < 0 || idx >= edconfig.number_of_rows) {
    return;
  }

  editor_free_row(&edconfig.current_rows[idx]);
  memmove(
    &edconfig.current_rows[idx],
    &edconfig.current_rows[idx + 1],
    sizeof(editor_row) * (edconfig.number_of_rows - idx - 1)
  );
  edconfig.number_of_rows--;
  edconfig.is_dirty++;
}

void editor_row_insert_char(editor_row *row, int idx, int c) {
  if (idx < 0 || idx > row->size) {
    idx = row->size;
  }

  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[idx + 1], &row->chars[idx], row->size - idx + 1);
  row->size++;
  row->chars[idx] = c;
  editor_update_row(row);
  edconfig.is_dirty++;
}

void editor_row_append_string(editor_row *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editor_update_row(row);
  edconfig.is_dirty++;
}

void editor_row_delete_char(editor_row *row, int idx) {
  if (idx < 0 || idx >= row->size) {
    return;
  }

  memmove(&row->chars[idx], &row->chars[idx + 1], row->size - idx);
  row->size--;
  editor_update_row(row);
  edconfig.is_dirty++;
}

void editor_insert_char(int c) {
  if (edconfig.cursor_y == edconfig.number_of_rows) {
    editor_insert_row(edconfig.cursor_y, "", 0);
  }

  editor_row_insert_char(
    &edconfig.current_rows[edconfig.cursor_y],
    edconfig.cursor_x,
    c
  );

  edconfig.cursor_x++;
}

void editor_insert_newline(void) {
  if (edconfig.cursor_x == 0) {
    editor_insert_row(edconfig.cursor_y, "", 0);
  } else {
    editor_row *current_row = &edconfig.current_rows[edconfig.cursor_y];
    editor_insert_row(
      edconfig.cursor_y + 1,
      &current_row->chars[edconfig.cursor_x],
      current_row->size - edconfig.cursor_x
    );
    current_row = &edconfig.current_rows[edconfig.cursor_y];
    current_row->size = edconfig.cursor_x;
    current_row->chars[current_row->size] = '\0';
    editor_update_row(current_row);
  }

  edconfig.cursor_y++;
  edconfig.cursor_x = 0;
}

void editor_delete_char(void) {
  if (
    edconfig.cursor_y == edconfig.number_of_rows ||
      (!edconfig.cursor_x && !edconfig.cursor_y)
  ) {
    return;
  }

  editor_row *current_row = &edconfig.current_rows[edconfig.cursor_y];

  if (edconfig.cursor_x > 0) {
    editor_row_delete_char(current_row, edconfig.cursor_x - 1);
    edconfig.cursor_x--;
  } else {
    edconfig.cursor_x = edconfig.current_rows[edconfig.cursor_y - 1].size;

    editor_row_append_string(
      &edconfig.current_rows[edconfig.cursor_y - 1],
      current_row->chars,
      current_row->size
    );

    editor_delete_row(edconfig.cursor_y);
    edconfig.cursor_y--;
  }
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

void editor_open(char *file_name) {
  free(edconfig.file_name);
  edconfig.file_name = strdup(file_name);

  FILE *file_processor = fopen(file_name, "r");

  if (!file_processor) {
    die("fopen");
  }

  char *line = NULL;
  size_t line_cap = 0;
  ssize_t line_length;

  while ((line_length = getline(&line, &line_cap, file_processor)) != -1) {
    while (line_length > 0 && (line[line_length - 1] == '\n' ||
                                line[line_length - 1] == '\r')) {
      line_length--;
    }

    editor_insert_row(edconfig.cursor_y, line, line_length);
  }

  free(line);
  fclose(file_processor);
  edconfig.is_dirty = 0;
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

void editor_save(void) {
  if (edconfig.file_name == NULL) {
    edconfig.file_name = editor_prompt("Save as: %s (esc to cancel)", NULL);
    if (edconfig.file_name == NULL) {
      editor_set_status_message("Save aborted.");
      return;
    }
  }

  int len;
  char *buffer = editor_rows_to_string(&len);

  int file_dump = open(
    edconfig.file_name,
    O_RDWR | O_CREAT,
    0644
  );

  if (file_dump != -1) {
    if (ftruncate(file_dump, len) != -1) {
      if (write(file_dump, buffer, len) == len) {
        close(file_dump);
        free(buffer);
        edconfig.is_dirty = 0;
        editor_set_status_message(
          "%d bytes written to disk",
          len
        );
        return;
      }
    }

    close(file_dump);
  }

  free(buffer);
  editor_set_status_message(
    "Can't save! I/O error: %s",
    strerror(errno)
  );
}

void editor_find_callback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_highlight_line;
  static char *saved_highlight = NULL;

  if (saved_highlight) {
    memcpy(
      edconfig.current_rows[saved_highlight_line].highlight,
      saved_highlight,
      edconfig.current_rows[saved_highlight_line].render_size
    );

    free(saved_highlight);
    saved_highlight = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1) {
    direction = 1;
  }

  int current_match = last_match;
  int i;

  for (i = 0; i < edconfig.number_of_rows; i++) {
    current_match += direction;

    if (current_match == -1) {
      current_match = edconfig.number_of_rows - 1;
    } else if (current_match == edconfig.number_of_rows) {
      current_match = 0;
    }

    editor_row *current_row = &edconfig.current_rows[current_match];
    char *match = strstr(current_row->render, (query));

    if (match) {
      last_match = current_match;
      edconfig.cursor_y = current_match;
      edconfig.cursor_x = editor_row_render_x_to_cursor_x(
        current_row,
        match - current_row->render
      );
      edconfig.row_offset = edconfig.number_of_rows;

      saved_highlight_line = current_match;
      saved_highlight = malloc(current_row->render_size);
      memcpy(saved_highlight, current_row->highlight, current_row->render_size);

      memset(
        &current_row->highlight[match - current_row->render],
        HIGHLIGHT_MATCH,
        strlen(query)
      );
      break;
    }
  }
}

void editor_find(void) {
  int saved_cursor_x = edconfig.cursor_x;
  int saved_cursor_y = edconfig.cursor_y;
  int saved_column_offset = edconfig.column_offset;
  int saved_row_offset = edconfig.row_offset;

  char *query = editor_prompt(
    "Search: %s (esc to cancel, arrows to navigate, enter to select)",
    editor_find_callback
  );

  if (query) {
    free(query);
  } else {
    edconfig.cursor_x = saved_cursor_x;
    edconfig.cursor_y = saved_cursor_y;
    edconfig.column_offset = saved_column_offset;
    edconfig.row_offset = saved_row_offset;
  }
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

void disable_raw_mode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &edconfig.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &edconfig.orig_termios) == -1) {
    die("tcgetattr");
  }

  atexit(disable_raw_mode);
  struct termios raw = edconfig.orig_termios;
  tcgetattr(STDIN_FILENO, &raw);

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag &= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
}

int get_cursor_position(int *rows, int *cols) {
  char cursor_buffer[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }
  
  while (i < sizeof cursor_buffer - 1) {
    if (read(STDIN_FILENO, &cursor_buffer[i], 1) != 1) {
      break;
    }
    
    if (cursor_buffer[i] == 'R') {
      break;
    }
    
    i++;
  }
  
  cursor_buffer[i] = '\0';
  
  if (cursor_buffer[0] != '\x1b' || cursor_buffer[1] != '[') {
    return -1;
  }
  
  if (sscanf(&cursor_buffer[2], "%d;%d", rows, cols) != 2) {
    return -1;
  }
  
  return 0;
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

int is_separator(int c) {
  return isspace(c) || c == '\0' ||
    strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editor_update_syntax(editor_row *row) {
  row->highlight = realloc(row->highlight, row->render_size);
  memset(row->highlight, HIGHLIGHT_NORMAL, row->render_size);

  int prev_separator = 1;

  int i = 0;
  while (i < row->size) {
    char c = row->render[i];
    unsigned char prev_highlight = (i > 0) ?
      row->highlight[i - 1] : HIGHLIGHT_NORMAL;

    if (
      (isdigit(c) && (prev_separator || prev_highlight == HIGHLIGHT_NUMBER)) ||
        (c == '.' && prev_highlight == HIGHLIGHT_NUMBER)
    ) {
      row->highlight[i] = HIGHLIGHT_NUMBER;
      i++;
      prev_separator = 0;
      continue;
    }

    prev_separator = is_separator(c);
    i++;
  }
}

int editor_syntax_to_color(int highlight) {
  switch (highlight) {
    case HIGHLIGHT_NUMBER:
      return 31;
    case HIGHLIGHT_MATCH:
      return 34;
    default:
      return 37;
  }
}

int editor_read_key(void) {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  if (c == '\x1b') {
    char escape_sequence[3];

    if (read(STDIN_FILENO, &escape_sequence[0], 1) != 1) {
      return '\x1b';
    }

    if (read(STDIN_FILENO, &escape_sequence[1], 1) != 1) {
      return '\x1b';
    }

    if (escape_sequence[0] == '[') {
      if (escape_sequence[1] >= '0' && escape_sequence[1] <= '9') {
        if (read(STDIN_FILENO, &escape_sequence[2], 1) != 1) {
          return '\x1b';
        }

        if (escape_sequence[2] == '~') {
          switch (escape_sequence[1]) {
            case '1':
              return HOME_KEY;
            case '3':
              return DEL_KEY;
            case '4':
              return END_KEY;
            case '5':
              return PAGE_UP;
            case '6':
              return PAGE_DOWN;
            case '7':
              return HOME_KEY;
            case '8':
              return END_KEY;
          }
        }
      } else {
        switch (escape_sequence[1]) {
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
        }
      }
    } else if (escape_sequence[0] == 'O') {
      switch (escape_sequence[1]) {
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
      }
    }

    return '\x1b';
  }

  return c;
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

void editor_move_cursor(int key) {
  editor_row *current_row = (
    edconfig.cursor_y >= edconfig.number_of_rows
  ) ? NULL : &edconfig.current_rows[edconfig.cursor_y];

  switch (key) {
    case ARROW_LEFT:
      if (edconfig.cursor_x != 0) {
        edconfig.cursor_x--;
      } else if (edconfig.cursor_y > 0) {
        edconfig.cursor_y--;
        edconfig.cursor_x = edconfig.current_rows[
          edconfig.cursor_y
        ].size;
      }

      break;
    case ARROW_RIGHT:
      if (current_row && edconfig.cursor_x < current_row->size) {
        edconfig.cursor_x++;
      } else if (current_row && edconfig.cursor_x == current_row->size) {
        edconfig.cursor_y++;
        edconfig.cursor_x = 0;
      }

      break;
    case ARROW_UP:
      if (edconfig.cursor_y != 0) {
        edconfig.cursor_y--;
      }

      break;
    case ARROW_DOWN:
      if (edconfig.cursor_y < edconfig.number_of_rows) {
        edconfig.cursor_y++;
      }

      break;
  }

  current_row = (
    edconfig.cursor_y >= edconfig.number_of_rows
  ) ? NULL : &edconfig.current_rows[edconfig.cursor_y];

  int current_row_length = current_row ? current_row->size : 0;

  if (edconfig.cursor_x > current_row_length) {
    edconfig.cursor_x = current_row_length;
  }
}

void editor_process_key_press(void) {
  static int quit_times = KOJI_QUIT_TIMES;
  int c = editor_read_key();

  switch (c) {
    case '\r':
      editor_insert_newline();
      break;

    case CTRL_KEY('x'):
      editor_save();
      editor_clear_screen();
      exit(0);

    case CTRL_KEY('q'):
      if (edconfig.is_dirty && quit_times) {
        editor_set_status_message(
          "File has unsaved changes, press Ctrl-Q again to quit"
        );
        quit_times--;
        return;
      }
      editor_clear_screen();
      exit(0);
      break;

    case CTRL_KEY('s'):
      editor_save();
      break;

    case CTRL_KEY('f'):
      editor_find();
      break;

    case HOME_KEY:
      edconfig.cursor_x = 0;
      break;

    case END_KEY:
      if (edconfig.cursor_y < edconfig.number_of_rows) {
        edconfig.cursor_x = edconfig.current_rows[
          edconfig.cursor_y
        ].size;
      }
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) {
        editor_move_cursor(ARROW_RIGHT);
      }

      editor_delete_char();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          edconfig.cursor_y = edconfig.row_offset;
        } else if (c == PAGE_DOWN) {
          edconfig.cursor_y = edconfig.row_offset +
            edconfig.screen_rows - 1;

          if (edconfig.cursor_y > edconfig.number_of_rows) {
            edconfig.cursor_y = edconfig.number_of_rows;
          }
        }

        int times = edconfig.screen_rows;
        while (times--) {
          if (c == PAGE_UP) {
            editor_move_cursor(ARROW_UP);
          } else if (c == PAGE_DOWN) {
            editor_move_cursor(ARROW_DOWN);
          }
        }
      }
      break;

    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_UP:
    case ARROW_DOWN:
      editor_move_cursor(c);
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editor_insert_char(c);
      break;
  }

  quit_times = KOJI_QUIT_TIMES;
}

void init_editor(void) {
  edconfig.cursor_x = 0;
  edconfig.cursor_y = 0;
  edconfig.render_x = 0;
  edconfig.row_offset = 0;
  edconfig.column_offset = 0;
  edconfig.number_of_rows = 0;
  edconfig.current_rows = NULL;
  edconfig.is_dirty = 0;
  edconfig.file_name = NULL;
  edconfig.status_message[0] = '\0';
  edconfig.status_message_time = 0;

  if (get_window_size(&edconfig.screen_rows, &edconfig.screen_columns) == -1) {
    die("get_window_size");
  }

  edconfig.screen_rows -= 2;
}

int main(int argc, char *argv[]) {
  enable_raw_mode();
  init_editor();

  if (argc >= 2) {
    editor_open(argv[1]);
  }

  editor_set_status_message("Help: press Ctrl-s to save, Ctrl-Q to quit");

  while (1) {
    editor_refresh_screen();
    editor_process_key_press();
  }
}
