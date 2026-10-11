; Use the opening fence's language to highlight the raw block contents.
(code_block
  language: (language) @injection.language
  (code_content) @injection.content)

; Double-backtick inline code also carries an explicit language.
(highlighted_inline_code
  language: (inline_language) @injection.language
  (code_content) @injection.content)
