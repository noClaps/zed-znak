.DEFAULT_GOAL := test
.PHONY: build test

SAMPLE ?= ../znak/test.zn
SAMPLE_PATH := $(abspath $(SAMPLE))

build:
	@cd tree-sitter-znak && tree-sitter generate --js-runtime=native

test: build
	@cd tree-sitter-znak && tree-sitter test
	@cd tree-sitter-znak && tree-sitter parse --quiet "$(SAMPLE_PATH)"

	@cd tree-sitter-znak && tree-sitter query --quiet ../languages/znak/highlights.scm "$(SAMPLE_PATH)"
	@cd tree-sitter-znak && tree-sitter query --quiet ../languages/znak/injections.scm "$(SAMPLE_PATH)"
	@cd tree-sitter-znak && tree-sitter query --quiet ../languages/znak/outline.scm "$(SAMPLE_PATH)"
	@cd tree-sitter-znak && tree-sitter query --quiet ../languages/znak/brackets.scm "$(SAMPLE_PATH)"
