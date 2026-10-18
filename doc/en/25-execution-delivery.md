# Execution and delivery

[Documentation index](README.md) · [Execution and delivery](../25-execution-delivery.md)

Magel supports interpreted execution, bytecode delivery and native AOT programs from the same source. Choose according to the target platform, host services and performance requirements.

| Mode | Example | Purpose |
| --- | --- | --- |
| Interpreter | `magel run hello.mgl` | Development, debugging and quick checks |
| Bytecode | `magel svmb hello.mgl` | Deliver `.svmb` to a matching StackVM host |
| AOT | `magel aot hello.mgl` | Produce a native executable and reduce interpreter overhead |

Run `magel check hello.mgl` for preprocessing, parsing and semantic checks. Create a project with `magel new hello-magel`; its entry is selected by `[magel] main` in `magel.toml`. See the [toolchain guide](09-toolchain.md) for options.

## Conditional values

Use an `if` expression to select a result. Only the selected branch executes. `choose`, `chooseLazy` and the ternary operator `? :` are not part of the current language interface.

```magel
int64 main() {
    bool ready = true;
    String label = if (ready) { "yes" } else { "no" };
    println("{}", label);
    return 0;
}
```

Statement branches produce results with `yield value;`. See [Custom error propagation and if expressions](26-error-propagation-if.md).

## Deployment boundaries

Bytecode and symbol compression make source less directly readable; they do not provide encryption or irreversible protection. Compiler and runtime assets must match. Verify that the host provides the required filesystem, network, peripheral and thread services.

Embedded deployments trim the runtime to the board budget; desktop services are not automatically available on boards. AOT requires a native C++ toolchain for the target. See [performance measurements](12-vm-performance.md) and [deployment](06-deployment.md).

## Build from source

```sh
sh gen.sh
make
sh check.sh
```

`gen.sh` requires the ParseEngine grammar generator. See [quickstart](01-quickstart.md) and the [toolchain guide](09-toolchain.md) for dependencies and platform instructions.
