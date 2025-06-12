#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/constants.h"
#include "../include/types.h"
#include "../include/utils.h"
#include "../include/write.h"
#include "../include/file.h"
#include "../include/render.h"
#include "../include/write.h"
#include "../include/search.h"

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
