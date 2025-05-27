#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)

typedef struct {
  int screen_rows;
  int screen_columns;
  struct termios orig_termios;
} editor_cofig;

editor_cofig edconfig;

void editor_clear_screen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void editor_draw_rows(void) {
  int y;
  for (y = 0; y < edconfig.screen_rows; y++) {
    write(STDOUT_FILENO, "~", 1);
    
    if (y < edconfig.screen_rows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void editor_refresh_screen(void) {
  editor_clear_screen();
  editor_draw_rows();
  write(STDOUT_FILENO, "\x1b[H", 3);
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

char editor_read_key(void) {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  return c;
}

void editor_process_key_press(void) {
  char c = editor_read_key();

  switch (c) {
    case CTRL_KEY('q'):
      editor_clear_screen();
      exit(0);
      break;
  }
}

void init_editor(void) {
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
