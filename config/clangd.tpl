# Canonical clangd config template for HPM projects.
# Distributed by: hpmdev sync-config   (board include path is detected per project)
# Source of truth: <env-repo>/config/clangd.tpl
---
Documentation:
  CommentFormat: Doxygen

CompileFlags:
  CompilationDatabase: build
  Add:
    - -I../Interface
    - -I../Board/@BOARDS@
    - -ferror-limit=0
  Remove:
    - -fno-shrink-wrap
    - -fstrict-volatile-bitfields
    - -fno-tree-switch-conversion
    - -mabi=ilp32d
    - -march=rv32imafdc

Diagnostics:
  UnusedIncludes: None
  MissingIncludes: None
  Includes:
    IgnoreHeader:
      - "bits/.+h"
  ClangTidy:
    FastCheckFilter: Loose
    Remove:
      - readability-inconsistent-declaration-parameter-name
  Suppress:
    - missing-includes
    - pp_file_not_found
    - drv_unknown_argument

Index:
  Background: Build

Hover:
  ShowAKA: true
