#ifndef TYPES
#define TYPES

#include <termios.h>
#include <time.h>

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
  HIGHLIGHT_COMMENT,
  HIGHLIGHT_MULTILINE_COMMENT,
  HIGHLIGHT_KEYWORD,
  HIGHLIGHT_TYPE,
  HIGHLIGHT_STRING,
  HIGHLIGHT_NUMBER,
  HIGHLIGHT_MATCH
};

typedef struct {
  char *buffer;
  int len;
} append_buffer;

typedef struct {
  int current_rows_idx;
  int size;
  int render_size;
  char *chars;
  char *render;
  unsigned char *highlight;
  int in_open_comment;
} editor_row;

typedef struct {
  char *file_type;
  char **file_match;
  char **keywords;
  int is_typed;
  char **types;
  char *single_line_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
} editor_syntax;

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
  editor_syntax *syntax;
  struct termios orig_termios;
} editor_config;

extern editor_config edconfig;

#endif
