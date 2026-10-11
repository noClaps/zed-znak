(comment) @comment

(heading_marker) @punctuation.special
(heading (text) @title)
(blockquote_marker) @punctuation.special
(attribution_marker) @punctuation.special
(unordered_list_marker) @punctuation.list_marker
(ordered_list_marker) @punctuation.list_marker
(horizontal_rule) @punctuation.special

(bold) @emphasis.strong
(italic) @emphasis
(strikethrough) @emphasis
(subscript) @emphasis
(superscript) @emphasis
(highlight) @emphasis.strong
(inline_code) @text.literal
(highlighted_inline_code) @text.literal
(code_content) @text.literal
(math_content) @text.literal
(inline_math) @text.literal

(link_label) @link_text
(url) @link_uri
(escape) @string.escape
(line_break) @string.escape

(frontmatter_start) @punctuation.delimiter
(frontmatter_end) @punctuation.delimiter
(frontmatter_key) @property
(frontmatter_value) @string
(code_fence_start) @punctuation.delimiter
(code_fence_end) @punctuation.delimiter
(container_fence_start) @punctuation.delimiter
(container_fence_end) @punctuation.delimiter
(math_fence_start) @punctuation.delimiter
(math_fence_end) @punctuation.delimiter
(container_name) @type
(container_title) @title
(language) @label
(inline_language) @label
(table_marker) @punctuation.delimiter
(table_separator) @punctuation.special
