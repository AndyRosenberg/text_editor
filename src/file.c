#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "../include/constants.h"
#include "../include/types.h"
#include "../include/utils.h"
#include "../include/render.h"
#include "../include/write.h"
#include "../include/syntax.h"

void editor_open(char *file_name) {
  free(edconfig.file_name);
  edconfig.file_name = strdup(file_name);

  editor_select_syntax_highlight();

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

    editor_insert_row(edconfig.number_of_rows, line, line_length);
  }

  free(line);
  fclose(file_processor);
  edconfig.is_dirty = 0;
}

void editor_save(void) {
  if (edconfig.file_name == NULL) {
    edconfig.file_name = editor_prompt("Save as: %s (esc to cancel)", NULL);
    if (edconfig.file_name == NULL) {
      editor_set_status_message("Save aborted.");
      return;
    }

    editor_select_syntax_highlight();
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
