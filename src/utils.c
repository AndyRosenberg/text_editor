#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/constants.h"
#include "../include/types.h"

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
