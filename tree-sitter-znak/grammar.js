/**
 * @file Znak grammar for tree-sitter
 * @author noClaps <contact@zerolimits.dev>
 * @license 0BSD
 */

/// <reference types="./dsl" />
// @ts-check

export default grammar({
  name: "znak",
  extras: () => [],

  externals: ($) => [
    $._paragraph_start,
    $.heading_marker,
    $.blockquote_marker,
    $.attribution_marker,
    $.unordered_list_marker,
    $.ordered_list_marker,
    $._indent,
    $._dedent,
    $.code_fence_start,
    $.code_content,
    $.code_fence_end,
    $.container_fence_start,
    $.container_fence_end,
    $.math_fence_start,
    $.math_content,
    $.math_fence_end,
    $.frontmatter_start,
    $.frontmatter_end,
    $.comment,
    $.horizontal_rule,
    $.table_marker,
    $._newline,
    $._bold_start,
    $._italic_start,
    $._superscript_start,
    $._inline_code_start,
    $._inline_math_start,
    $._bracket_start,
    $._bare_link_start,
    $._image_start,
    $.raw_fence_start,
    $.raw_content,
    $.raw_fence_end,
    $._inline_raw_start,
  ],

  rules: {
    source_file: ($) =>
      seq(optional($.frontmatter), repeat(choice($._block, $._newline))),
    frontmatter: ($) =>
      seq($.frontmatter_start, repeat($.frontmatter_entry), $.frontmatter_end),
    frontmatter_entry: ($) =>
      seq(
        field("key", $.frontmatter_key),
        "=",
        optional(field("value", $.frontmatter_value)),
        $._newline,
      ),
    frontmatter_key: () => /[^=\r\n]+/,
    frontmatter_value: () => /[^\r\n]+/,

    _block: ($) =>
      choice(
        $.comment,
        $.heading,
        $.blockquote,
        $.unordered_list,
        $.ordered_list,
        $.code_block,
        $.container,
        $.math_block,
        $.raw_block,
        $.table,
        $.horizontal_rule,
        $.paragraph,
      ),
    heading: ($) => seq($.heading_marker, optional($._inline), $._newline),
    paragraph: ($) =>
      prec.right(repeat1(seq($._paragraph_start, $._inline, $._newline))),
    blockquote: ($) =>
      prec.right(seq(repeat1($.blockquote_line), optional($.attribution))),
    blockquote_line: ($) =>
      seq(
        $.blockquote_marker,
        choice($.heading, seq(optional($._inline), $._newline)),
      ),
    attribution: ($) =>
      prec.right(
        repeat1(seq($.attribution_marker, optional($._inline), $._newline)),
      ),

    unordered_list: ($) => prec.right(repeat1($.unordered_list_item)),
    ordered_list: ($) => prec.right(repeat1($.ordered_list_item)),
    unordered_list_item: ($) =>
      prec.right(
        seq(
          $.unordered_list_marker,
          optional($._inline),
          $._newline,
          repeat($._newline),
          optional($.list_item_body),
        ),
      ),
    ordered_list_item: ($) =>
      prec.right(
        seq(
          $.ordered_list_marker,
          optional($._inline),
          $._newline,
          repeat($._newline),
          optional($.list_item_body),
        ),
      ),
    list_item_body: ($) =>
      seq($._indent, repeat1(choice($._block, $._newline)), $._dedent),

    code_block: ($) =>
      seq(
        $.code_fence_start,
        optional(field("language", $.language)),
        $._newline,
        optional($.code_content),
        $.code_fence_end,
      ),
    language: () => /[^\r\n]+/,
    container: ($) =>
      seq(
        $.container_fence_start,
        field("name", $.container_name),
        optional(seq($._space, field("title", $.container_title))),
        $._newline,
        repeat(choice($._block, $._newline)),
        $.container_fence_end,
      ),
    container_name: () => /[^ \t\r\n]+/,
    container_title: ($) => $._inline,
    math_block: ($) =>
      seq($.math_fence_start, optional($.math_content), $.math_fence_end),
    raw_block: ($) =>
      seq($.raw_fence_start, optional($.raw_content), $.raw_fence_end),
    table: ($) => prec.right(repeat1(choice($.table_row, $.table_separator))),
    table_row: ($) =>
      seq($.table_marker, repeat1(seq($.table_cell, "|")), $._newline),
    table_cell: ($) => repeat1($._inline_nonpipe),
    table_separator: ($) =>
      seq(
        $.table_marker,
        token(prec(2, /[ \t]*-+[ \t]*\|([ \t]*-+[ \t]*\|)*/)),
        $._newline,
      ),

    _inline: ($) => repeat1($._inline_element),
    _inline_element: ($) => choice($._inline_nonpipe, alias("|", $.text)),
    _inline_nonpipe: ($) =>
      choice(
        $.inline_raw,
        $.text,
        $.escape,
        $.line_break,
        $.bold,
        $.italic,
        $.superscript,
        $.inline_code,
        $.inline_math,
        $.link,
        $.bare_link,
        $.image,
        $.bracketed_text,
        $.punctuation,
      ),
    text: () => /[^\\*_^`$\[\]<>!|{}\r\n]+/,
    punctuation: () => token(prec(-1, /[\\*_^`$\[\]<>!{}]/)),
    escape: () => token(prec(2, /\\[^ \r\n]/)),
    line_break: () => "\\ ",
    bold: ($) => formatted($, $._bold_start, "*", "bold"),
    italic: ($) => formatted($, $._italic_start, "_", "italic"),
    superscript: ($) => formatted($, $._superscript_start, "^", "superscript"),
    inline_code: ($) =>
      seq(
        $._inline_code_start,
        optional(alias(/[^`\r\n]+/, $.code_content)),
        "`",
      ),
    inline_math: ($) =>
      seq(
        $._inline_math_start,
        optional(alias(/[^$\r\n]+/, $.math_content)),
        "$",
      ),
    inline_raw: ($) =>
      seq(
        $._inline_raw_start,
        optional(alias(/([^}\r\n]|}[^}\r\n])+/, $.raw_content)),
        "}}",
      ),
    link: ($) =>
      prec(
        1,
        seq(
          $._bracket_start,
          optional(field("label", $.link_label)),
          "]",
          $.bare_link,
        ),
      ),
    image: ($) =>
      seq(
        $._image_start,
        optional(field("caption", $.link_label)),
        "]",
        $.bare_link,
      ),
    link_label: ($) =>
      repeat1(
        choice(
          $.text,
          $.escape,
          $.line_break,
          $.bold,
          $.italic,
          $.superscript,
          $.inline_code,
          $.inline_math,
          $.inline_raw,
          $.link,
          $.bare_link,
          $.bracketed_text,
          alias(token(prec(-1, /[^\]\\\r\n]/)), $.punctuation),
        ),
      ),
    bracketed_text: ($) => seq($._bracket_start, optional($.link_label), "]"),
    bare_link: ($) => seq($._bare_link_start, field("url", $.url), ">"),
    url: () => /[^>\r\n]+/,
    _space: () => /[ \t]+/,
  },
});

// An enclosing formatting delimiter terminates the span, so it must not be a
// fallback punctuation token within that span.
/**
 * @param {GrammarSymbols<string>} $
 * @param {RuleOrLiteral} start
 * @param {string} delimiter
 * @param {string} ownRule
 */
function formatted($, start, delimiter, ownRule) {
  const formats = ["bold", "italic", "superscript"];
  const punctuation = "\\*_^`$[]<>!|{}".replace(delimiter, "");
  const escaped = punctuation.replace(/[\\\]\[\^\-]/g, "\\$&");
  return seq(
    start,
    repeat(
      choice(
        $.text,
        $.escape,
        $.line_break,
        ...formats.filter((name) => name !== ownRule).map((name) => $[name]),
        $.inline_raw,
        $.inline_code,
        $.inline_math,
        $.link,
        $.bare_link,
        $.image,
        $.bracketed_text,
        alias(token(prec(-1, new RegExp(`[${escaped}]`))), $.punctuation),
      ),
    ),
    delimiter,
  );
}
