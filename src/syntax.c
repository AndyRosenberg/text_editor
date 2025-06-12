#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "../include/constants.h"
#include "../include/hldb.h"
#include "../include/types.h"

int is_separator(int c) {
  return isspace(c) || c == '\0' ||
    strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editor_update_syntax(editor_row *row) {
  row->highlight = realloc(row->highlight, row->render_size);
  memset(row->highlight, HIGHLIGHT_NORMAL, row->render_size);

  if (edconfig.syntax == NULL) {
    return;
  }

  char **keywords = edconfig.syntax->keywords;
  char **types = edconfig.syntax->types;
  int is_typed = edconfig.syntax->is_typed;

  char *sl_comment_start = edconfig.syntax->single_line_comment_start;
  char *ml_comment_start = edconfig.syntax->multiline_comment_start;
  char *ml_comment_end = edconfig.syntax->multiline_comment_end;

  int sl_comment_start_length = sl_comment_start ? strlen(sl_comment_start) : 0;
  int ml_comment_start_length = ml_comment_start ? strlen(ml_comment_start) : 0;
  int ml_comment_end_length = ml_comment_end ? strlen(ml_comment_end) : 0;

  int prev_separator = 1;
  int in_string = 0;
  int in_ml_comment = (
    row->current_rows_idx > 0 &&
      edconfig.current_rows[row->current_rows_idx - 1].in_open_comment
  );

  int i = 0;
  while (i < row->render_size) {
    char c = row->render[i];
    unsigned char prev_highlight = (i > 0) ?
      row->highlight[i - 1] : HIGHLIGHT_NORMAL;

    if (sl_comment_start_length && !in_string && !in_ml_comment) {
      if (!strncmp(&row->render[i], sl_comment_start, sl_comment_start_length)) {
        memset(&row->highlight[i], HIGHLIGHT_COMMENT, row->render_size - i);
        break;
      }
    }

    if (ml_comment_start_length && ml_comment_end_length && !in_string) {
      if (in_ml_comment) {
        row->highlight[i] = HIGHLIGHT_MULTILINE_COMMENT;

        if (!strncmp(&row->render[i], ml_comment_end, ml_comment_end_length)) {
          memset(&row->highlight[i], HIGHLIGHT_MULTILINE_COMMENT, ml_comment_end_length);
          i += ml_comment_end_length;
          in_ml_comment = 0;
          prev_separator = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], ml_comment_start, ml_comment_start_length)) {
        memset(&row->highlight[i], HIGHLIGHT_MULTILINE_COMMENT, ml_comment_start_length);
        i += ml_comment_start_length;
        in_ml_comment = 1;
        continue;
      }
    }

    if (edconfig.syntax->flags & HIGHLIGHT_STRINGS_FLAG) {
      if (in_string) {
        row->highlight[i] = HIGHLIGHT_STRING;

        if (c == '\\' && i + 1 < row->render_size) {
          row->highlight[i + 1] = HIGHLIGHT_STRING;
          i += 2;
          continue;
        }

        if (c == in_string) {
          in_string = 0;
        }

        i++;
        prev_separator = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->highlight[i] = HIGHLIGHT_STRING;
          i++;
          continue;
        }
      }
    }

    if (edconfig.syntax->flags & HIGHLIGHT_NUMBERS_FLAG) {
      if (
        (isdigit(c) && (prev_separator || prev_highlight == HIGHLIGHT_NUMBER)) ||
          (c == '.' && prev_highlight == HIGHLIGHT_NUMBER)
      ) {
        row->highlight[i] = HIGHLIGHT_NUMBER;
        i++;
        prev_separator = 0;
        continue;
      }
    }

    if (prev_separator) {
      int j;
      int k;

      for (j = 0; keywords[j]; j++) {
        int keyword_len = strlen(keywords[j]);

        if (!strncmp(
          &row->render[i],
          keywords[j],
          keyword_len
        ) &&
          is_separator(row->render[i + keyword_len])) {
            memset(
              &row->highlight[i],
              HIGHLIGHT_KEYWORD,
              keyword_len
            );
        }
      }

      if (is_typed) {
        for (k = 0; types[k]; k++) {
          int type_len = strlen(types[k]);

          if (!strncmp(
            &row->render[i],
            types[k],
            type_len
          ) &&
            is_separator(row->render[i + type_len])) {
              memset(
                &row->highlight[i],
                HIGHLIGHT_TYPE,
                type_len
              );
          }
        }
      }

      if (keywords[j] != NULL && (!is_typed || types[k] != NULL)) {
        prev_separator = 0;
        continue;
      }
    }

    prev_separator = is_separator(c);
    i++;
  }

  int changed_ml_comment_status = (
    row->in_open_comment != in_ml_comment
  );

  row->in_open_comment = in_ml_comment;

  if (changed_ml_comment_status && row->current_rows_idx + 1 < edconfig.number_of_rows) {
    editor_update_syntax(&edconfig.current_rows[
      row->current_rows_idx + 1
    ]);
  }
}

int editor_syntax_to_color(int highlight) {
  switch (highlight) {
    case HIGHLIGHT_COMMENT:
    case HIGHLIGHT_MULTILINE_COMMENT:
      return 36;
    case HIGHLIGHT_KEYWORD:
      return 33;
    case HIGHLIGHT_TYPE:
      return 32;
    case HIGHLIGHT_STRING:
      return 35;
    case HIGHLIGHT_NUMBER:
      return 31;
    case HIGHLIGHT_MATCH:
      return 34;
    default:
      return 37;
  }
}

void editor_select_syntax_highlight(void) {
  edconfig.syntax = NULL;
  if (edconfig.file_name == NULL) {
    return;
  }

  char *file_ext = strchr(edconfig.file_name, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    editor_syntax *syntax = &HLDB[j];
    unsigned int i = 0;

    while (syntax->file_match[i]) {
      int is_ext = syntax->file_match[i][0] == '.';

      if (
        (is_ext && file_ext && !strcmp(file_ext, syntax->file_match[i])) ||
          (!is_ext && strstr(edconfig.file_name, syntax->file_match[i]))
      ) {
        edconfig.syntax = syntax;

        int file_row;
        for (file_row = 0; file_row < edconfig.number_of_rows; file_row++) {
          editor_update_syntax(&edconfig.current_rows[file_row]);
        }

        return;
      }

      i++;
    }
  }
}

