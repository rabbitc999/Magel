# 01 Five-minute quickstart

Goal: run your first Magel program within five minutes. You do not need to understand value semantics
and references first — that is [Core concepts](03-core-concepts.md).

---

## 1. Install

Two ways; pick one:

**Option 1: use a release package**

The release package `dist/magel-<version>-<platform>-<arch>.tar.gz` contains `bin/` (`magel`,
`magel-emit`, `magel-generator`, `magel-stackvm`, …), `doc/`, `VERSION`, `CHANGELOG.md` and `LICENSE`.
Unpack it and put `bin/` on your PATH.

**Option 2: build from source**

```sh
sh gen.sh      # generate the front end from magel.gram with IELR (needs the ParseEngine generator)
make           # build magel and build/magel-frontend/libparser.a
```

`gen.sh` needs a [ParseEngine](https://github.com/rabbitc999/ParseEngine) grammar generator.
The easiest route is to drop it in the project root as `./ParseEngine` (add `.exe` on Windows);
alternatively leave it in the neighbouring ParseEngine repository and `gen.sh` will find
`../ParseEngine/build/bin/` automatically.

On Windows, if no generator is found, `gen.sh` tries the latest release on [Gitee](https://gitee.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe) and then [GitHub](https://github.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe), and keeps whichever succeeds first as `ParseEngine.exe` in the project root. Existing generators are reused. Other platforms require a native executable, optionally selected through `ENGINE`.

Verify the install:

```sh
magel help
```

> On Windows the binary is `magel.exe`. Under Git Bash / MSYS write `./magel`, which resolves to
> `magel.exe`; a bare `magel` goes through PATH lookup.

---

## 2. The first program

Create `hello.mgl`:

```magel
int64 main() {
    println("Hello, Magel!");
    return 0;
}
```

Three things worth noting:

- The entry function is always `main`, returning `int64` or `void`. Normal completion of `void main` exits with code 0.
- `println` comes from the automatically loaded core library; its full name is `println`, and
  the prefix can be dropped when there is no name clash.
- Semicolons and braces work as in C-family languages — deliberately so. Magel borrows C's expression
  and block syntax.

---

## 3. Run it

```sh
magel run hello.mgl
```

Output:

```text
Hello, Magel!
```

Handing a source file straight to `magel` also interprets it, and everything after it is passed to
`main` as program arguments:

```sh
magel hello.mgl Alice "two words"
```

When you need arguments, write the entry as:

```magel
int64 main(List<String> arguments) {
    println("hello, {}", arguments[0]);
    return 0;
}
```

---

## 4. What happened

`magel run` does four things, all **inside one process**:

1. **Preprocess** — handles `%include`, `%define`, `%if` and friends;
2. **Parse and type-check** — syntax, types and reference lifetimes are checked here; nothing runs if
   this fails;
3. **Lower to quadruples** — the front end turns the program into StackVM's intermediate
   representation;
4. **Interpret** — run it directly in the current process.

To stop after the first two steps (no execution, no output files):

```sh
magel check hello.mgl
```

---

## 5. Next

| You want | Where |
| --- | --- |
| Learn the language features | [Language tour](02-language-tour.md) |
| Hand the program to a customer / flash a device | [Delivery security](05-delivery-security.md) → [Cross-platform deployment](06-deployment.md) |
| See what the standard library offers | [Standard library overview](07-stdlib-overview.md) |
| Start a real project | `magel new my-app`; configuration in [Toolchain](09-toolchain.md) |

---

## 6. Command cheat-sheet

| Task | Command |
| --- | --- |
| Interpret | `magel run src/main.mgl` |
| Check only, do not run | `magel check src/main.mgl` |
| JSON diagnostics (for editors) | `magel check src/main.mgl json` |
| Export `.svmb` bytecode | `magel svmb src/main.mgl out build/app` |
| Export a native executable | `magel aot src/main.mgl out build/app.exe opt 2` |
| Create a project | `magel new hello` |
| Run tests | `magel test tests` |
| Format check / write | `magel fmt check src` / `magel fmt write src` |
| Show help | `magel help` |

More subcommands, `magel.toml` configuration and compile options: [Toolchain](09-toolchain.md).

> Reading the English edition? Snippets are identical to the Chinese edition and are executed before
> publication. See the [Chinese edition](../01-quickstart.md).


Generated parser sources, headers and reports live in `source/`. Object files and the parser archive live in `build/magel-frontend/`, keeping the repository root clear.
