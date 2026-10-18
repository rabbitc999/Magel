# LSP incremental hints and caching

`magel lsp` uses six queues for completion/signatures, hover/definitions, semantic tokens/outlines, workspace queries, macro previews, and compiler diagnostics. The protocol thread continues to receive edits and cancellations. JSON-RPC IDs pair responses with requests, and a mutex protects complete output frames. [中文](../lsp.md)

## Types, macros and safe refactoring

Completion, hover, definitions and refactoring share binding scans and type queries. Original document URIs and declaration offsets distinguish shadowed names. Queries cover `for(auto item in values)` element types, generic fields and method results, inference of direct function type parameters, explicit generic arguments, nested `fn` results and contextual anonymous function parameters. Uncertain types do not produce inlay hints; compiler diagnostics remain authoritative.

`auto` hover shows the complete inferred type on either the variable or the `auto` keyword, including generic arguments and nested function signatures. Function types additionally describe parameters and return types. Unknown inference is reported explicitly. `textDocument/inlayHint` displays inferred types for `auto` bindings and contextual parameters; VS Code users can disable `magel.inlayHints.enabled` or use the editor's inlay hint settings.

Macro signature help reuses the compiler's character grouper for all four bracket kinds, custom orders and repeated groups. It highlights the active group and reports each group's item count. An incomplete final group can receive hints without relaxing compilation. Order mismatches identify expected and actual brackets. Preview still runs the real preprocessor. Definition navigation maps expanded lines to original source lines, using column zero when no precise column map exists. Generated temporary names expose integer slots, expansion IDs and navigation to original slots, including `$0`, `$10` and arbitrary length integers.

References and rename use uniquely resolved declarations for local variables, parameters, fields, functions, types and cross-module source references, including unsaved buffers. Rename emits versioned `WorkspaceEdit.documentChanges`, and the client checks open document versions again before applying. Strings, comments and generated names are untouched. Reserved symbols, ambiguous overloads, collisions and unresolvable occurrences in macro templates reject the entire rename. Generic parameters, macro parameters and embedded standard library declarations without original source locations are currently excluded, as are syntax fragments without unique bindings.

Ownership diagnostics preserve compiler codes, severity and primary ranges. When an original binding is identified, related information points to its declaration and relevant explicit capture, `move` or `defer` usage. It explains the primary diagnostic and never changes `move` to `copy` automatically.

## Edits and hints

VS Code sends actual document versions and UTF-16 edit ranges. Immutable source snapshots share lexical, line, scope, and declaration indices, built once per version. Hint workers share standard library declarations. Hover and completion can return before full compiler diagnostics finish.

Diagnostics have a 250 millisecond debounce and retain the newest pending task. Saving starts a check immediately. New checks cancel old ones and terminate obsolete compiler subprocesses. Editor cancellation tokens become `$/cancelRequest` notifications. A new request of the same hint category for the same document cancels its predecessor. Stale responses and diagnostics do not replace current editor content; closing a file clears its diagnostics.

Checks use the original source path and stdin text. Unsaved documents overlay imports and includes without writing source files. Diagnostic caches validate source, dependencies, configuration, and lock file fingerprints. Dependency edits refresh their users; closing an unsaved dependency restores disk content. File watchers report `.mgl`, `.inc`, `.toml`, and `.lock` additions, changes, and removals.

The compiler still performs complete type and borrow checking. Fast hints and final diagnostics can finish at different times. `fn move() { a }` takes ownership of a, so reading a afterwards still produces MGL1205. Capture keywords have contextual hover text, and MGL1205 marks the offending variable on its reported line.

## Cache location and cleanup

Caches live in `build/lsp/` beside the magel executable. With the bundled compiler this is the extension's `bin/build/lsp/`; with `magel.path` it is beside the configured compiler. The project directory does not select the cache location.

| Directory | Contents |
| --- | --- |
| `index/` | Document declaration indices validated against compiler identity and source fingerprints |
| `diagnostics/` | Complete compiler reports and dependency fingerprints |
| `preprocess/` | Existing incremental compilation preprocessing and module caches |

Cleanup runs at startup and is scheduled every fifteen minutes when messages arrive. Regular files unused for seven days are removed. Successful index and diagnostic reuse refreshes timestamps at most once an hour. Above 256 MiB, oldest files are evicted toward 192 MiB. Files newer than ten minutes are protected, so active writes may temporarily exceed the quota. Cleanup stays inside `build/lsp/`, skips symlinks, and preserves other build files.

Memory indices use estimated 64 MiB and 256 entry eviction thresholds; disk source text uses 32 MiB and 512 entries. Least recently used records are evicted first. Active request snapshots remain alive until their requests finish, and one oversized file may exceed a threshold. These thresholds control caches rather than total process memory. Unwritable or damaged persistent caches fall back to analysis. Once the service exits, `build/lsp/` can be removed manually and will be regenerated. Published VSIX packages exclude runtime caches.

Binding scans have a separate limit of sixteen entries and sixteen MiB in total; individual entries above one MiB are not cached.

## Verification

[tests/magel_lsp_performance.py](../../../tests/magel_lsp_performance.py) covers concurrent hints/diagnostics, UTF-16 edits, unsaved dependencies and close rollback, report reuse, restart recovery, corrupted cache fallback, age cleanup, and quota cleanup. [Capture tests](../../../tests/magel_lsp_lambda_capture.py) verify hover and diagnostic ranges. [Client tests](../../../vscode/test/lspCancellation.js) cover out of order responses and cancellation listener disposal.

[Semantic/refactoring tests](../../../tests/magel_lsp_semantic_features.py) cover complete type hover, contextual anonymous functions, repeated macro groups, incomplete calls, integer slot origins, cross-module versioned rename, shadowing and collision rejection, related cleanup locations and a latency gate for two thousand references. [Editor provider tests](../../../vscode/test/semanticProviders.js) check actual API conversion, stale version rejection and the hint setting.

On the same Windows machine with a 600 function file, first hover took approximately 1.52 seconds in 0.17.1 and 0.16 seconds in 0.17.2. Repeated semantic token requests took about 0.70 and 0.02 seconds. These measurements describe language service requests; overall editor responsiveness also depends on project size, extensions, and machine load.

[Command line manual](README.md) · [Documentation index](../../en/README.md)
