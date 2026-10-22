# 01 Five-minute quickstart

Goal: run your first ChaLi program within five minutes. You do not need to understand value semantics
and references first — that is [Core concepts](03-core-concepts.md).

---

## 1. Install

Two ways; pick one:

**Option 1: use a release package**

**Option 2: build from source**

```sh
sh gen.sh      # generate the front end from chali.gram with IELR (needs the ParseEngine generator)
make           # build chali and build/chali-frontend/libparser.a
```

`gen.sh` needs a [ParseEngine](https://github.com/rabbitc999/ParseEngine) grammar generator.
The easiest route is to drop it in the project root as `./ParseEngine` (add `.exe` on Windows);
alternatively leave it in the neighbouring ParseEngine repository and `gen.sh` will find
`../ParseEngine/build/bin/` automatically.

On Windows, if no generator is found, `gen.sh` tries the latest release on [Gitee](https://gitee.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe) and then [GitHub](https://github.com/rabbitc999/ParseEngine/releases/download/latest/ParseEngine.exe), and keeps whichever succeeds first as `ParseEngine.exe` in the project root. Existing generators are reused. Other platforms require a native executable, optionally selected through `ENGINE`.

Verify the install:

```sh
chali help
```

> On Windows the binary is `ChaLi.exe`. Under Git Bash / MSYS write `./chali`, which resolves to
> `ChaLi.exe`; a bare `chali` goes through PATH lookup.

---

## 2. The first program

Create `hello.chl`:

```chali
int64 main() {
    println("Hello, ChaLi!");
    return 0;
}
```

Three things worth noting:

- The entry function is always `main`, returning `int64` or `void`. Normal completion of `void main` exits with code 0.
- `println` comes from the automatically loaded core library; its full name is `println`, and
  the prefix can be dropped when there is no name clash.
- Semicolons and braces work as in C-family languages — deliberately so. ChaLi borrows C's expression
  and block syntax.

---

## 3. Run it

```sh
chali run hello.chl
```

Output:

```text
Hello, ChaLi!
```

Handing a source file straight to `chali` also interprets it, and everything after it is passed to
`main` as program arguments:

```sh
chali hello.chl Alice "two words"
```

When you need arguments, write the entry as:

```chali
int64 main(List<String> arguments) {
    println("hello, {}", arguments[0]);
    return 0;
}
```

---

## 4. What happened

`chali run` does four things, all **inside one process**:

1. **Preprocess** — handles `%define`, `%if` and friends;
2. **Parse and type-check** — syntax, types and reference lifetimes are checked here; nothing runs if
   this fails;
3. **Lower to quadruples** — the front end turns the program into StackVM's intermediate
   representation;
4. **Interpret** — run it directly in the current process.

To stop after the first two steps (no execution, no output files):

```sh
chali check hello.chl
```

---

## 5. Next

| You want | Where |
| --- | --- |
| Learn the language features | [Language tour](02-language-tour.md) |
| Deploy the program to a target host | [Delivery security](05-delivery-security.md) → [Cross-platform deployment](06-deployment.md) |
| See what the standard library offers | [Standard library overview](07-stdlib-overview.md) |
| Start a real project | `chali new my-app`; configuration in [Toolchain](09-toolchain.md) |

---

## 6. Command cheat-sheet

| Task | Command |
| --- | --- |
| Interpret | `chali run src/main.chl` |
| Check only, do not run | `chali check src/main.chl` |
| JSON diagnostics (for editors) | `chali check src/main.chl json` |
| Export `.svmb` bytecode | `chali svmb src/main.chl out build/app` |
| Create a project | `chali new hello` |
| Run tests | `chali test tests` |
| Format check / write | `chali fmt check src` / `chali fmt write src` |
| Show help | `chali help` |

More subcommands, `chali.toml` configuration and compile options: [Toolchain](09-toolchain.md).

> Reading the English edition? Snippets are identical to the Chinese edition and are executed before
> publication. See the [Chinese edition](../01-quickstart.md).

Generated parser sources, headers and reports live in `source/`. Object files and the parser archive live in `build/chali-frontend/`, keeping the repository root clear.
