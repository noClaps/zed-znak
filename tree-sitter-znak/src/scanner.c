#include "tree_sitter/parser.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum Token {
  PARAGRAPH_START,
  HEADING_MARKER,
  BLOCKQUOTE_MARKER,
  ATTRIBUTION_MARKER,
  UNORDERED_LIST_MARKER,
  ORDERED_LIST_MARKER,
  INDENT,
  DEDENT,
  CODE_FENCE_START,
  CODE_CONTENT,
  CODE_FENCE_END,
  CONTAINER_FENCE_START,
  CONTAINER_FENCE_END,
  MATH_FENCE_START,
  MATH_CONTENT,
  MATH_FENCE_END,
  FRONTMATTER_START,
  FRONTMATTER_END,
  COMMENT,
  HORIZONTAL_RULE,
  TABLE_MARKER,
  NEWLINE,
  BOLD_START,
  ITALIC_START,

  SUPERSCRIPT_START,

  INLINE_CODE_START,

  INLINE_MATH_START,
  BRACKET_START,
  BARE_LINK_START,
  IMAGE_START,
  RAW_FENCE_START,
  RAW_CONTENT,
  RAW_FENCE_END,
  INLINE_RAW_START,
};

#define MAX_DEPTH 100

typedef struct {
  uint16_t code_fence;
  uint16_t containers[MAX_DEPTH];
  uint16_t indents[MAX_DEPTH];
  uint8_t container_depth;
  uint8_t indent_depth;
  bool math;
  bool frontmatter;
  bool eof_newline;
  bool raw;
} Scanner;

static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }
static bool eol(TSLexer *lexer) {
  return lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
         lexer->eof(lexer);
}
static void finish_line(TSLexer *lexer) {
  if (lexer->lookahead == '\r')
    advance(lexer);
  if (lexer->lookahead == '\n')
    advance(lexer);
  lexer->mark_end(lexer);
}

void *tree_sitter_znak_external_scanner_create(void) {
  return calloc(1, sizeof(Scanner));
}
void tree_sitter_znak_external_scanner_destroy(void *payload) { free(payload); }

unsigned tree_sitter_znak_external_scanner_serialize(void *payload,
                                                     char *buffer) {
  Scanner *s = payload;
  unsigned n = 0;
  buffer[n++] = s->code_fence & 255;
  buffer[n++] = s->code_fence >> 8;
  buffer[n++] = s->container_depth;
  buffer[n++] = s->indent_depth;
  buffer[n++] = s->math;
  buffer[n++] = s->frontmatter;
  buffer[n++] = s->eof_newline;
  buffer[n++] = s->raw;
  for (unsigned i = 0; i < s->container_depth; i++) {
    buffer[n++] = s->containers[i] & 255;
    buffer[n++] = s->containers[i] >> 8;
  }
  for (unsigned i = 0; i < s->indent_depth; i++) {
    buffer[n++] = s->indents[i] & 255;
    buffer[n++] = s->indents[i] >> 8;
  }
  return n;
}

void tree_sitter_znak_external_scanner_deserialize(void *payload,
                                                   const char *buffer,
                                                   unsigned length) {
  Scanner *s = payload;
  memset(s, 0, sizeof(*s));
  if (length < 8)
    return;
  const uint8_t *b = (const uint8_t *)buffer;
  s->code_fence = b[0] | (b[1] << 8);
  s->container_depth = b[2];
  s->indent_depth = b[3];
  s->math = b[4];
  s->frontmatter = b[5];
  s->eof_newline = b[6];
  s->raw = b[7];
  if (s->container_depth > MAX_DEPTH || s->indent_depth > MAX_DEPTH ||
      length != 8u + 2u * (s->container_depth + s->indent_depth)) {
    memset(s, 0, sizeof(*s));
    return;
  }
  unsigned n = 8;
  for (unsigned i = 0; i < s->container_depth; i++, n += 2)
    s->containers[i] = b[n] | (b[n + 1] << 8);
  for (unsigned i = 0; i < s->indent_depth; i++, n += 2)
    s->indents[i] = b[n] | (b[n + 1] << 8);
}

// Raw blocks stop only at an exact fence on a line of its own. Shorter
// backtick fences therefore remain code, rather than opening nested blocks.
static bool raw_block(Scanner *s, TSLexer *lexer, const bool *valid,
                      enum Token content) {
  bool math = content == MATH_CONTENT;
  bool raw = content == RAW_CONTENT;
  enum Token end = raw ? RAW_FENCE_END : math ? MATH_FENCE_END : CODE_FENCE_END;
  if (!valid[content] && !valid[end])
    return false;
  int delimiter = raw ? '}' : math ? '$' : '`';
  unsigned length = raw || math ? 2 : s->code_fence;
  bool consumed = false;
  lexer->mark_end(lexer);
  while (!lexer->eof(lexer)) {
    unsigned indent = 0;
    while (lexer->lookahead == ' ') {
      indent++;
      advance(lexer);
    }
    unsigned expected_indent =
        s->indent_depth ? s->indents[s->indent_depth - 1] : 0;
    unsigned count = 0;
    while (lexer->lookahead == delimiter) {
      count++;
      advance(lexer);
    }
    if (count == length && indent == expected_indent && eol(lexer)) {
      if (consumed && valid[content]) {
        lexer->result_symbol = content;
        return true;
      }
      if (!valid[end])
        return false;
      finish_line(lexer);
      if (raw)
        s->raw = false;
      else if (math)
        s->math = false;
      else
        s->code_fence = 0;
      lexer->result_symbol = end;
      return true;
    }
    while (!eol(lexer))
      advance(lexer);
    finish_line(lexer);
    consumed = true;
  }
  if (consumed && valid[content]) {
    lexer->result_symbol = content;
    return true;
  }
  return false;
}

// Unpaired punctuation stays text instead of consuming subsequent blocks.
static bool inline_start(TSLexer *lexer, const bool *valid) {
  int delimiter = lexer->lookahead;
  enum Token symbol;
  if (delimiter == '{' && valid[INLINE_RAW_START]) {
    advance(lexer);
    if (lexer->lookahead != '{')
      return false;
    advance(lexer);
    lexer->mark_end(lexer);
    while (!eol(lexer)) {
      if (lexer->lookahead == '}') {
        advance(lexer);
        if (lexer->lookahead == '}') {
          lexer->result_symbol = INLINE_RAW_START;
          return true;
        }
      } else {
        advance(lexer);
      }
    }
    return false;
  }
  if (delimiter == '[' || delimiter == '<' || delimiter == '!') {
    bool image = delimiter == '!';
    symbol = image              ? IMAGE_START
             : delimiter == '[' ? BRACKET_START
                                : BARE_LINK_START;
    if (!valid[symbol])
      return false;
    advance(lexer);
    if (image) {
      if (lexer->lookahead != '[')
        return false;
      advance(lexer);
    }
    lexer->mark_end(lexer);
    if (delimiter == '<') {
      while (!eol(lexer) && lexer->lookahead != '>')
        advance(lexer);
      if (lexer->lookahead != '>')
        return false;
    } else {
      unsigned depth = 1;
      while (!eol(lexer) && depth) {
        if (lexer->lookahead == '[')
          depth++;
        if (lexer->lookahead == ']')
          depth--;
        advance(lexer);
      }
      if (depth)
        return false;
      if (image) {
        if (lexer->lookahead != '<')
          return false;
        advance(lexer);
        while (!eol(lexer) && lexer->lookahead != '>')
          advance(lexer);
        if (lexer->lookahead != '>')
          return false;
      }
    }
    lexer->result_symbol = symbol;
    return true;
  }
  switch (delimiter) {
  case '*':
    symbol = BOLD_START;
    break;
  case '_':
    symbol = ITALIC_START;
    break;
  case '^':
    symbol = SUPERSCRIPT_START;
    break;

  case '`':
    symbol = INLINE_CODE_START;
    break;
  case '$':
    symbol = INLINE_MATH_START;
    break;
  default:
    return false;
  }
  advance(lexer);

  if (!valid[symbol])
    return false;
  lexer->mark_end(lexer);
  while (!eol(lexer)) {
    if (lexer->lookahead == delimiter) {
      lexer->result_symbol = symbol;
      return true;
    } else {
      advance(lexer);
    }
  }
  return false;
}

bool tree_sitter_znak_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid) {
  Scanner *s = payload;
  // Do not modify scanner state during Tree-sitter's all-symbol error recovery.
  if (valid[INDENT] && valid[CODE_CONTENT] && valid[FRONTMATTER_START])
    return false;
  if (s->code_fence && (valid[CODE_CONTENT] || valid[CODE_FENCE_END]))
    return raw_block(s, lexer, valid, CODE_CONTENT);
  if (s->math && (valid[MATH_CONTENT] || valid[MATH_FENCE_END]))
    return raw_block(s, lexer, valid, MATH_CONTENT);
  if (s->raw && (valid[RAW_CONTENT] || valid[RAW_FENCE_END]))
    return raw_block(s, lexer, valid, RAW_CONTENT);

  if (!valid[PARAGRAPH_START] &&
      (!valid[HEADING_MARKER] || lexer->lookahead != '=') &&
      lexer->lookahead > 0 && lexer->lookahead < 128 &&
      strchr("*_^`$[<!{", lexer->lookahead) &&
      (valid[BOLD_START] || valid[ITALIC_START] || valid[SUPERSCRIPT_START] ||
       valid[INLINE_CODE_START] || valid[INLINE_MATH_START] ||
       valid[BRACKET_START] || valid[BARE_LINK_START] || valid[IMAGE_START] ||
       valid[INLINE_RAW_START]))
    return inline_start(lexer, valid);

  lexer->mark_end(lexer);
  if (lexer->eof(lexer)) {
    if (s->indent_depth && valid[DEDENT]) {
      s->indent_depth--;
      lexer->result_symbol = DEDENT;
      return true;
    }
    if (valid[NEWLINE] && !s->eof_newline) {
      s->eof_newline = true;
      lexer->result_symbol = NEWLINE;
      return true;
    }
    return false;
  }
  if (valid[NEWLINE] &&
      (lexer->lookahead == '\n' || lexer->lookahead == '\r')) {
    finish_line(lexer);
    s->eof_newline = false;
    lexer->result_symbol = NEWLINE;
    return true;
  }

  unsigned column = lexer->get_column(lexer);
  unsigned current = s->indent_depth ? s->indents[s->indent_depth - 1] : 0;
  bool line_start = column == 0 || (s->indent_depth && column == current);
  if (column == 0) {
    unsigned indent = 0;
    while (lexer->lookahead == ' ') {
      indent++;
      lexer->advance(lexer, true);
    }
    lexer->mark_end(lexer);
    bool tabs = false;
    while (lexer->lookahead == '\t' || (tabs && lexer->lookahead == ' ')) {
      tabs = true;
      lexer->advance(lexer, true);
    }
    if (tabs && !eol(lexer)) {
      // Tabs can make a blank line, but only literal spaces nest list items.
      // Keep nonblank tab-prefixed lines as paragraph content.
      if (indent < current && valid[DEDENT]) {
        s->indent_depth--;
        lexer->result_symbol = DEDENT;
        return true;
      }
      if (indent >= current + 2 && valid[INDENT] &&
          s->indent_depth < MAX_DEPTH) {
        s->indents[s->indent_depth++] = indent;
        lexer->result_symbol = INDENT;
        return true;
      }
      if (valid[PARAGRAPH_START]) {
        lexer->result_symbol = PARAGRAPH_START;
        return true;
      }
      return false;
    }
    if (!eol(lexer)) {
      if (indent < current && valid[DEDENT]) {
        s->indent_depth--;
        lexer->result_symbol = DEDENT;
        return true;
      }
      if (indent >= current + 2 && valid[INDENT] &&
          s->indent_depth < MAX_DEPTH) {
        s->indents[s->indent_depth++] = indent;
        lexer->mark_end(lexer);
        lexer->result_symbol = INDENT;
        return true;
      }
    }
    lexer->mark_end(lexer);
    if (valid[NEWLINE] &&
        (lexer->lookahead == '\n' || lexer->lookahead == '\r')) {
      finish_line(lexer);
      s->eof_newline = false;
      lexer->result_symbol = NEWLINE;
      return true;
    }
  }
  if (!line_start && !valid[HEADING_MARKER])
    return false;

  int c = lexer->lookahead;
  if (line_start && c == '{' && valid[RAW_FENCE_START]) {
    advance(lexer);
    if (lexer->lookahead == '{') {
      advance(lexer);
      while (!eol(lexer))
        advance(lexer);
      s->raw = true;
      finish_line(lexer);
      lexer->result_symbol = RAW_FENCE_START;
      return true;
    }
  } else if (c == '=' && valid[HEADING_MARKER]) {
    do {
      advance(lexer);
    } while (lexer->lookahead == '=');
    if (lexer->lookahead == ' ') {
      advance(lexer);
      lexer->mark_end(lexer);
      lexer->result_symbol = HEADING_MARKER;
      return true;
    }
  } else if (line_start && c == '>' && valid[BLOCKQUOTE_MARKER]) {
    advance(lexer);
    if (lexer->lookahead == ' ' || eol(lexer)) {
      if (lexer->lookahead == ' ')
        advance(lexer);
      lexer->mark_end(lexer);
      lexer->result_symbol = BLOCKQUOTE_MARKER;
      return true;
    }
    if (lexer->lookahead == '-' && valid[ATTRIBUTION_MARKER]) {
      advance(lexer);
      if (lexer->lookahead != '-')
        return false;
      advance(lexer);
      if (lexer->lookahead != ' ')
        return false;
      advance(lexer);
      lexer->mark_end(lexer);
      lexer->result_symbol = ATTRIBUTION_MARKER;
      return true;
    }
  } else if (line_start && c == '>' && valid[ATTRIBUTION_MARKER]) {
    advance(lexer);
    if (lexer->lookahead != '-')
      return false;
    advance(lexer);
    if (lexer->lookahead != '-')
      return false;
    advance(lexer);
    if (lexer->lookahead != ' ')
      return false;
    advance(lexer);
    lexer->mark_end(lexer);
    lexer->result_symbol = ATTRIBUTION_MARKER;
    return true;
  } else if (line_start && (c == '-' || c == '+')) {
    unsigned count = 0;
    do {
      count++;
      advance(lexer);
    } while (lexer->lookahead == c);
    if (c == '-' && count == 3 && eol(lexer) &&
        (valid[FRONTMATTER_START] ||
         (s->frontmatter && valid[FRONTMATTER_END]))) {
      lexer->result_symbol =
          s->frontmatter ? FRONTMATTER_END : FRONTMATTER_START;
      s->frontmatter = !s->frontmatter;
      finish_line(lexer);
      return true;
    }
    if (c == '-' && count > 3 && eol(lexer) && valid[HORIZONTAL_RULE]) {
      finish_line(lexer);
      lexer->result_symbol = HORIZONTAL_RULE;
      return true;
    }
    enum Token marker = c == '-' ? UNORDERED_LIST_MARKER : ORDERED_LIST_MARKER;
    if (count == 1 && lexer->lookahead == ' ' && valid[marker]) {
      advance(lexer);
      lexer->mark_end(lexer);
      lexer->result_symbol = marker;
      return true;
    }
  } else if (line_start && c == '`' && valid[CODE_FENCE_START]) {
    unsigned count = 0;
    do {
      count++;
      advance(lexer);
    } while (lexer->lookahead == '`');
    if (count >= 3 && count <= UINT16_MAX) {
      s->code_fence = count;
      lexer->mark_end(lexer);
      lexer->result_symbol = CODE_FENCE_START;
      return true;
    }
  } else if (line_start && c == ':' &&
             (valid[CONTAINER_FENCE_START] || valid[CONTAINER_FENCE_END])) {
    unsigned count = 0;
    do {
      count++;
      advance(lexer);
    } while (lexer->lookahead == ':');
    if (s->container_depth && count == s->containers[s->container_depth - 1] &&
        eol(lexer) && valid[CONTAINER_FENCE_END]) {
      s->container_depth--;
      finish_line(lexer);
      lexer->result_symbol = CONTAINER_FENCE_END;
      return true;
    }
    if (count >= 3 && count <= UINT16_MAX && lexer->lookahead == ' ' &&
        valid[CONTAINER_FENCE_START] && s->container_depth < MAX_DEPTH) {
      do {
        advance(lexer);
      } while (lexer->lookahead == ' ');
      s->containers[s->container_depth++] = count;
      lexer->mark_end(lexer);
      lexer->result_symbol = CONTAINER_FENCE_START;
      return true;
    }
  } else if (line_start && c == '$' && valid[MATH_FENCE_START]) {
    advance(lexer);
    if (lexer->lookahead == '$') {
      advance(lexer);
      if (eol(lexer)) {
        s->math = true;
        finish_line(lexer);
        lexer->result_symbol = MATH_FENCE_START;
        return true;
      }
    }
  } else if (line_start && c == '\\' && valid[COMMENT]) {
    advance(lexer);
    if (lexer->lookahead == '\\') {
      advance(lexer);
      if (lexer->lookahead == ' ') {
        while (!eol(lexer))
          advance(lexer);
        finish_line(lexer);
        lexer->result_symbol = COMMENT;
        return true;
      }
    }
  } else if (line_start && c == '|' && valid[TABLE_MARKER]) {
    advance(lexer);
    if (lexer->lookahead == ' ') {
      lexer->mark_end(lexer);
      lexer->result_symbol = TABLE_MARKER;
      return true;
    }
  }
  if (line_start && valid[PARAGRAPH_START]) {
    lexer->result_symbol = PARAGRAPH_START;
    return true;
  }
  return false;
}
