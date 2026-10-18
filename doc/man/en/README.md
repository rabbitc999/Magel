# Command line manual

API queries require the explicit `api` command: `magel man api std` lists standard-library modules, and `magel man api std::text::Text` describes a type. Commands such as `magel man std` or `magel man std::text::Text` report a usage error and show the corrected command.

`magel man` queries the standard library index embedded in the executable. Normal queries need neither a network connection, Python, nor the source repository. The full library reference remains in [doc/library](../../library/README.md). This directory documents command behaviour, the JSON structure, and [LSP incremental hints and caching](lsp.md). [中文](../README.md)

## Queries

```sh
magel man
magel man api std::text
magel man api std::text::Text
magel man api std::text::Text::split
magel man api --search delimiter
magel man api --search split --exact
magel man --json std::text::Text::split
magel man api std::text::Text::split --run
magel man --lang en std::text::Text::split
```

| Form | Output |
| --- | --- |
| No target | Modules grouped by core/types, collections/iteration, text/data, mathematics, I/O/integration, concurrency/events, and memory/system |
| Module | Overview and one signature per type, function, constant, or field |
| Module::Type | Type documentation, fields, and methods |
| Module::Symbol or Module::Type::Method | Signatures, parameters, defaults, return values, error boundaries, usage, and existing complete examples; each overload is listed |
| `--search KEYWORD` | Searches module and symbol names, parameters, descriptions, comments, and example code; case insensitive substring matching by default |
| `--exact` | Searches complete fields, with case sensitivity; `split` can match a method's final name segment |
| `--json` | Outputs JSON only, including diagnostics on failure |
| `--lang zh|en` | Overrides the toolchain display language for headings and diagnostics |
| `--run` | Executes the first complete example associated with the target and appends its output |

Standard library qualified targets are accepted, and a module prefix can omit `std::`. An unknown target produces up to eight suggestions and exit status 1. A search keyword cannot be combined with a specific target.

Interactive terminals use colour unless `NO_COLOR` is set. Text over 24 lines uses `less -R` when less is available on PATH. Redirected and JSON output use plain UTF-8. Windows console APIs enable UTF-8 and virtual terminal colours.

Library prose currently retains its existing Chinese source. English mode translate the interface and diagnostics and identify the prose source language. JSON `language` identifies the display language; `sourceLanguage` identifies the documentation source language.

## Running examples

Examples come from existing regression samples, including inputs, arguments, compiler flags, and expected output. `--run` writes and runs the complete sample in a temporary directory and removes the directory afterwards. A sample may cover several APIs; a short webpage fragment is not treated as a complete program.

Missing examples produce MGL1702. Compilation failure, runtime failure, or a 60 second timeout produces MGL1703. JSON `execution` contains `example`, `exitCode`, `stdout`, and `stderr`; stdout preserves platform line endings. Missing targets produce MGL1701 with suggestions in `candidates` and diagnostic notes. Invalid command arguments exit with status 2.

## JSON structure

The structure version is 1. Field names follow the compiler diagnostic naming style.

| Field | Meaning |
| --- | --- |
| `schemaVersion` | Integer, currently 1 |
| `language`, `sourceLanguage` | Display and documentation source languages |
| `kind`, `target` | Query category and resolved target |
| `modules` | Module records: name, title, documentation, capability, sourceLanguage, and examples |
| `symbols` | Existing symbol records: module, name, kind, owner, signature, type, documentation, parameters, examples, and other index fields |
| `diagnostics` | Compiler-compatible code, severity, message, span, source, and notes |
| `groups` | Module groups for a query without a target |
| `query`, `candidates`, `execution` | Present for search, missing targets, or example execution |

Function parameters provide name, type, documentation, and default, with an empty default string when none is declared. Returns provide type and documentation. Errors provide module boundary descriptions, preconditions, and panic information. Example objects provide id, source, code, flags, arguments, input, expected, and language. Detailed queries expand example IDs into objects. Consumers should accept additional optional fields.

## Maintaining the data

```sh
magel man --update
python tools/library_reference.py --check
sh gen.sh
make -j4
```

`--update` is a development path requiring the source repository and Python. It runs [tools/library_reference.py](../../../tools/library_reference.py) to regenerate the [library reference](../../library/README.md), API indices, and the declarations shared with LSP. Rebuild magel to embed the result. Normal queries never launch Python.

CI checks generated data with `--check`. [tests/magel_man.py](../../../tests/magel_man.py) covers modules, types, methods, search, JSON, three display languages, suggestions, and examples. `magel help` explains toolchain commands; `magel man` documents library APIs.

[Documentation index](../../en/README.md)

## Embedded book, keywords and AI SKILL

`magel man book` exports all book Markdown and images into `book/` under the calling directory, preserving relative image paths and listing chapters; `magel man book 6` displays one. Appendix IDs `A` and `B`, file names, and full titles are accepted. `magel man keyword` lists keywords; `magel man keyword break` displays grammar, explanations, and examples generated from the authoritative grammar and language reference. `magel man skill` emits the complete AI SKILL, suitable for redirecting to `SKILL.md`. The VS Code package carries identical content at `skills/magel-programming/SKILL.md`.

Data is compressed into the executable at build time. Queries require neither a repository nor network access. Book and SKILL prose remains Chinese; terminal Markdown retains image references, while the PDF includes the images. All three query forms accept `--json`; `--run` and `--exact` are rejected. Missing chapters or keywords return MGL1701. Run `python tools/embed_manual.py` after source changes; `--check` verifies synchronization and `gen.sh` refreshes the embedded assets.

Export updates embedded book files without deleting unrelated files; output paths must stay within the calling directory’s book folder.

Single-chapter terminal output renders images as `[图片]\(images/name)` links. JSON and exported Markdown retain the original image syntax.

`magel man` and `magel man help` show usage. `magel man api` lists API categories; `magel man api XXX` queries the embedded `doc/library/` reference.


`magel man macro` displays embedded help for definitions, invocations, ordered bracket groups, independent variadic parameters and multiline braces. It also supports `--json`.
