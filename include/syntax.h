#ifndef SYNTAX
#define SYNTAX

#include "types.h"

int is_separator(int c);
void editor_update_syntax(editor_row *row);
int editor_syntax_to_color(int highlight);
void editor_select_syntax_highlight(void);

#endif
