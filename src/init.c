#include "../include/constants.h"
#include "../include/types.h"
#include "../include/utils.h"
#include "../include/render.h"

editor_config edconfig;

void init_editor(void) {
  edconfig.cursor_x = 0;
  edconfig.cursor_y = 0;
  edconfig.render_x = 0;
  edconfig.row_offset = 0;
  edconfig.column_offset = 0;
  edconfig.number_of_rows = 0;
  edconfig.current_rows = NULL;
  edconfig.is_dirty = 0;
  edconfig.file_name = NULL;
  edconfig.status_message[0] = '\0';
  edconfig.status_message_time = 0;
  edconfig.syntax = NULL;

  if (get_window_size(&edconfig.screen_rows, &edconfig.screen_columns) == -1) {
    die("get_window_size");
  }

  edconfig.screen_rows -= 2;
}
