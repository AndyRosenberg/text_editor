#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define KOJI_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define APPEND_BUFFER_INIT { NULL, 0 }

enum MOVEMENT_KEYS {
  ARROW_LEFT = 1000,
  ARROW_RIGHT = 1001,
  ARROW_UP = 1002,
  ARROW_DOWN = 1003
};

typedef struct {
  char *buffer;
  int len;
} append_buffer;

typedef struct {
  int cursor_x;
  int cursor_y;
  int screen_rows;
  int screen_columns;
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

void editor_draw_rows(append_buffer *ab) {
  int y;
  for (y = 0; y < edconfig.screen_rows; y++) {
    if (y == edconfig.screen_rows / 3) {
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

    ab_append(ab, "\x1b[K", 3);

    if (y < edconfig.screen_rows - 1) {
      ab_append(ab, "\r\n", 2);
    }
  }
}

void editor_refresh_screen(void) {
  append_buffer ab = APPEND_BUFFER_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[H", 3);

  editor_draw_rows(&ab);

  char cursor_buffer[32];

  snprintf(
    cursor_buffer,
    sizeof(cursor_buffer),
    "\x1b[%d;%dH",
    edconfig.cursor_y + 1,
    edconfig.cursor_x + 1
  );

  ab_append(&ab, cursor_buffer, strlen(cursor_buffer));
  ab_append(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.buffer, ab.len);
  ab_free(&ab);
}

void die(const char *s) {
  editor_clear_screen();
  perror(s);
  exit(1);
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
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }
  
  while (i < sizeof buf - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    
    if (buf[i] == 'R') {
      break;
    }
    
    i++;
  }
  
  buf[i] = '\0';
  
  if (buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }
  
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
    return -1;
  }
  
  return 0;
}

int get_window_size(int *rows, int *cols) {
  struct winsize ws;
  
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return -1;
    }

    return get_cursor_position(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
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
      switch (escape_sequence[1]) {
        case 'A': 
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
      }
    }

    return '\x1b';
  }

  return c;
}

void editor_move_cursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      edconfig.cursor_x--;
      break;
    case ARROW_RIGHT:
      edconfig.cursor_x++;
      break;
    case ARROW_UP:
      edconfig.cursor_y--;
      break;
    case ARROW_DOWN:
      edconfig.cursor_y++;
      break;
  }
}

void editor_process_key_press(void) {
  int c = editor_read_key();

  switch (c) {
    case CTRL_KEY('q'):
      editor_clear_screen();
      exit(0);
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

  if (get_window_size(&edconfig.screen_rows, &edconfig.screen_columns) == -1) {
    die("get_window_size");
  }
}

int main(void) {
  enable_raw_mode();
  init_editor();

  while (1) {
    editor_refresh_screen();
    editor_process_key_press();
  }
}
