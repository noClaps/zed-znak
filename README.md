# Znak extension for Zed

Language support for the [Znak](https://codeberg.org/noClaps/znak) markup language.

> [!NOTE]
> This extension and Tree-sitter grammar is vibe-coded

## Install

1. Ensure `extension.toml` references a Git revision containing the generated grammar. Zed clones that revision; uncommitted files are not included.
2. Run **zed: install dev extension** in Zed and select this directory.
3. Open a `.zn` or `.znak` file.

For embedded code highlighting, the code block's language must be available in Zed.
