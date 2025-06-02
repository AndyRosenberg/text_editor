#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define KOJI_VERSION "0.0.1"
#define KOJI_TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)
#define APPEND_BUFFER_INIT { NULL, 0 }

enum MOVEMENT_KEYS {
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

typedef struct {
  char *buffer;
  int len;
} append_buffer;

typedef struct {
  int size;
  int render_size;
  char *chars;
  char *render;
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
  struct termios orig_termios;
} editor_cofig;

editor_cofig edconfig;

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

      ab_append(
        ab,
        &edconfig.current_rows[file_row].render[edconfig.column_offset],
        last_row_length
      );
    }

    // append newlines and clear other terminal contents
    ab_append(ab, "\x1b[K", 3);
    if (y < edconfig.screen_rows - 1) {
      ab_append(ab, "\r\n", 2);
    }
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
}

void editor_append_row(char *s, size_t len) {
  edconfig.current_rows = realloc(
    edconfig.current_rows,
    sizeof(editor_row) * (edconfig.number_of_rows + 1)
  );

  int last_row_n = edconfig.number_of_rows;
  edconfig.current_rows[last_row_n].size = len;
  edconfig.current_rows[last_row_n].chars = malloc(len + 1);

  memcpy(edconfig.current_rows[last_row_n].chars, s, len);

  edconfig.current_rows[last_row_n].chars[len] = '\0';

  edconfig.current_rows[last_row_n].render_size = 0;
  edconfig.current_rows[last_row_n].render = NULL;
  editor_update_row(&edconfig.current_rows[last_row_n]);

  edconfig.number_of_rows++;
}

void editor_open(char *file_name) {
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

    editor_append_row(line, line_length);
  }

  free(line);
  fclose(file_processor);
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
  int c = editor_read_key();

  switch (c) {
    case CTRL_KEY('q'):
      editor_clear_screen();
      exit(0);
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
  }
}

void init_editor(void) {
  edconfig.cursor_x = 0;
  edconfig.cursor_y = 0;
  edconfig.render_x = 0;
  edconfig.row_offset = 0;
  edconfig.column_offset = 0;
  edconfig.number_of_rows = 0;
  edconfig.current_rows = NULL;

  if (get_window_size(&edconfig.screen_rows, &edconfig.screen_columns) == -1) {
    die("get_window_size");
  }
}

int main(int argc, char *argv[]) {
  enable_raw_mode();
  init_editor();

  if (argc >= 2) {
    editor_open(argv[1]);
  }

  while (1) {
    editor_refresh_screen();
    editor_process_key_press();
  }
}
