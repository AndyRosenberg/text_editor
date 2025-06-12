#include <string.h>
#include <stdlib.h>
#include "../include/constants.h"
#include "../include/types.h"
#include "../include/render.h"

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
