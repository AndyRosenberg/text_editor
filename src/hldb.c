#include "../include/constants.h"
#include "../include/types.h"

char *C_HIGHLIGHT_EXTENSIONS[] = { ".c", ".h", ".cpp", NULL };

char *C_HIGHLIGHT_KEYWORDS[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "#define", "#include", NULL
};

char *C_HIGHLIGHT_TYPES[] = {
  "int", "long", "double", "float", "char", "unsigned",
  "signed", "void", "size_t", "time_t", NULL
};

char *RUBY_HIGHLIGHT_EXTENSIONS[] = {
  ".rb", ".erb", ".rake", ".gemspec", ".ru", NULL
};

char *RUBY_KEYWORDS[] = {
  "__ENCODING__", "__LINE__", "__FILE__",
  "BEGIN", "END", "alias", "and", "begin", "break", "case", "class", "def",
  "defined?", "do", "else", "elsif", "end", "ensure", "false", "for", "if",
  "in", "module", "next", "nil", "not", "or", "redo", "rescue", "retry",
  "return", "self", "super", "then", "true", "undef", "unless", "until",
  "when", "while", "yield", NULL
};

editor_syntax HLDB[] = {
  {
    .file_type="c",
    .file_match=C_HIGHLIGHT_EXTENSIONS,
    .keywords=C_HIGHLIGHT_KEYWORDS,
    .is_typed=1,
    .types=C_HIGHLIGHT_TYPES,
    .single_line_comment_start="//",
    .multiline_comment_start="/*",
    .multiline_comment_end="*/",
    .flags=HIGHLIGHT_NUMBERS_FLAG | HIGHLIGHT_STRINGS_FLAG
  },
  {
    .file_type="ruby",
    .file_match=RUBY_HIGHLIGHT_EXTENSIONS,
    .keywords=RUBY_KEYWORDS,
    .is_typed=0,
    .single_line_comment_start="#",
    .multiline_comment_start="=begin",
    .multiline_comment_end="=end",
    .flags=HIGHLIGHT_NUMBERS_FLAG | HIGHLIGHT_STRINGS_FLAG
  }
};

const size_t HLDB_ENTRIES = (sizeof(HLDB) / sizeof(HLDB[0]));
