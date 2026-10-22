# 40 Name Freeze: The Five Letters of ChaLi

The language name **ChaLi** is not a pretty acronym — each letter locks one **non-reversible**
guarantee. This page fixes those five guarantees and gives each a check you can run right now.
If a check ever stops holding, the letter no longer means what it says.

Two neighbouring pages freeze the other layers: semantic contracts cover how values flow and when
they are released; the syntax freeze list covers how tokens are spelled and shaped. This page
covers what the *name* promises.

## 0 The name itself is frozen too

| Item | Frozen spelling | Note |
| --- | --- | --- |
| Language name | `ChaLi` (capital C and L) | Used in docs, help and the manual |
| Source suffix | `.chl` | How the toolchain recognizes source files |
| Diagnostic prefix | `CHL` | e.g. `CHL1204`; code values belong to the diagnostic system, this page fixes the prefix |
| Executable | `ChaLi.exe` (Windows) | The command is still spelled lowercase `chali`; Windows matches it case-insensitively |
| Project files | `chali.toml` / `chali.lock` | Manifest and dependency lock |

The historical name survives only in `CHANGELOG.md` — that is change history, not something to
rewrite.

## 1 C — Copy (deep-copy value semantics)

**Contract**: assignment copies in full, never shares memory. Large objects go by explicit reference.

**Checks**

- Ordinary assignment and pass-by-value copy; editing the copy leaves the source untouched.
  Strings, containers and owning fields all follow value semantics
  ([03 Core concepts §1](03-core-concepts.md)).
- Runnable samples: `example/samples/struct_value.chl`, `example/samples/struct_container_field.chl`.

**What counts as a violation**

- Editing a copy after assignment also changes the source (memory was quietly shared).
- Passing large objects by reference by default, with no explicit reference type in the signature —
  that turns "copy" into an implicit, opt-in behaviour.

## 2 H — Hygienic (hygienic macros)

**Contract**: compile-time substitution that never captures or pollutes outer variables by accident.

**Checks**

- Names inside a macro body are renamed hygienically and never collide with same-named variables at
  the call site: runnable sample `example/samples/macro_gensym.chl`.
- Macros can be called across modules by qualified name (`$std::lang::assert(...)`) under the usual
  visibility rules (`std::lang` is always visible; other modules must be imported first):
  runnable sample `example/samples/macro_qualified.chl`. An undefined one reports at the call site.
- Macro rules in full: [24 Literals, operators and macros](24-literals-operators-macros.md).

**What counts as a violation**

- Expansion captures a same-named variable from the call site, or leaks macro-internal temporaries
  into the caller's scope.
- A qualified call bypasses visibility (reaching another module's macro without importing it).

## 3 A — Any (universal box / type erasure)

**Contract**: holds any type when no generic constraint applies.

**Checks**

- `Any` erases the concrete type on store and validates it against the declared type on retrieval:
  runnable sample `example/samples/any_generics.chl`; rules in
  [22 Any and generic data](22-any-generics-data.md).

**What counts as a violation**

- `Any` accepting only a fixed set of types (that is a union, not a universal box).
- Skipping the check on retrieval, so a type mismatch becomes undefined behaviour at run time.

## 4 L — Lifetime (borrow checker)

**Contract**: a reference never outlives its source; otherwise compilation fails (`CHL1204`).

**Checks**

- The borrow checker rejects three shapes at compile time (see `source/chali_borrow_checker.hpp`):
  a stored reference outliving its source, a reference in a return value outliving a local source,
  and an `Any` by-value argument hiding a non-global reference source (pass an explicit reference
  type instead).
- Legal uses: `example/samples/borrowed_aggregate_return.chl`,
  `example/samples/borrowed_index_values.chl`. Rejected ones:
  `example/samples-error/iter_erase_borrowed.chl`,
  `example/samples-error/iter_transfer_borrowed.chl`.

**What counts as a violation**

- Letting an outliving reference compile and deferring it to a dangling reference at run time.
- Downgrading `CHL1204` to a warning — the contract says "fails to compile".

## 5 I — Interface (runtime polymorphism)

**Contract**: a shared contract dispatched on the object's real type at run time.

**Checks**

- Interface methods dispatch on the receiver's actual type at run time: runnable samples
  `example/samples/interface.chl`, `example/samples/interface_protocols.chl`,
  `example/samples/interface_view_ownership.chl`. Default methods and inheritance:
  `example/samples/interface_defaults.chl`, `example/samples/interface_inherit.chl`.

**What counts as a violation**

- Collapsing into static dispatch (the target fixed at compile time, actual type ignored at run time).
- Letting a partially implemented object call an unimplemented method through the interface silently.

## 6 Renaming and acceptance

Each letter maps to the checks above; if a check fails, the language is not ChaLi. Whenever the
language name, source suffix, diagnostic prefix or executable name changes, these must move together
— no half renames:

- The five-letter banner printed at the top of help and the manual (`PrintUsage` in
  `source/chali_driver.cpp`, the `man` usage in `tools/native_man.hpp`), with both languages
  supplied by `source/i18n/messages.tsv`.
- Every version string (`VERSION`, `source/chali_package.hpp`, the language server,
  `tools/chali_dev.py`, the VS Code extension's `chaliCompilerVersion`).
- Command and suffix examples in the toolchain and quickstart pages.
