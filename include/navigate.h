#ifndef NAVIGATE
#define NAVIGATE

int get_cursor_position(int *rows, int *cols);
void editor_move_cursor(int key);
void editor_process_key_press(void);

#endif
