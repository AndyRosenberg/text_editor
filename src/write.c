#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../include/constants.h"
#include "../include/types.h"
#include "../include/utils.h"
#include "../include/syntax.h"

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

  for (int j = idx + 1; j <= edconfig.number_of_rows; j++) {
    edconfig.current_rows[j].current_rows_idx++;
  }

  edconfig.current_rows[idx].current_rows_idx = idx;

  edconfig.current_rows[idx].size = len;
  edconfig.current_rows[idx].chars = malloc(len + 1);

  memcpy(edconfig.current_rows[idx].chars, s, len);

  edconfig.current_rows[idx].chars[len] = '\0';

  edconfig.current_rows[idx].render_size = 0;
  edconfig.current_rows[idx].render = NULL;
  edconfig.current_rows[idx].highlight = NULL;
  edconfig.current_rows[idx].in_open_comment = 0;
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

  for (int j = idx; j < edconfig.number_of_rows - 1; j++) {
    edconfig.current_rows[j].current_rows_idx--;
  }

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
