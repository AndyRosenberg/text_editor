#ifndef WRITE
#define WRITE

#include "types.h"

void editor_update_row(editor_row *row);
void editor_insert_row(int idx, char *s, size_t len);
void editor_free_row(editor_row *row);
void editor_delete_row(int idx);
void editor_row_insert_char(editor_row *row, int idx, int c);
void editor_row_append_string(editor_row *row, char *s, size_t len);
void editor_row_delete_char(editor_row *row, int idx);
void editor_insert_char(int c);
void editor_insert_newline(void);
void editor_delete_char(void);
int editor_read_key(void);

#endif
