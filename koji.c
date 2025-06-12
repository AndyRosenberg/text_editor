#include "include/constants.h"
#include "include/types.h"
#include "include/utils.h"
#include "include/init.h"
#include "include/render.h"
#include "include/navigate.h"
#include "include/file.h"

int main(int argc, char *argv[]) {
  enable_raw_mode();
  init_editor();

  if (argc >= 2) {
    editor_open(argv[1]);
  }

  editor_set_status_message("Help: press Ctrl-s to save, Ctrl-Q to quit");

  while (1) {
    editor_refresh_screen();
    editor_process_key_press();
  }
}
