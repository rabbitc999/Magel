# 25 Execution and deployment boundaries

Programs execute only in the VM. `run` compiles and executes source; `emit` writes quadruples and data; `svmb` writes bytecode for a compatible VM.

| Task | Command or API | Result |
| --- | --- | --- |
| Run source | `chali run app.chl` | VM execution and exit status |
| Check | `chali check app.chl` | Syntax, type, borrow and semantic diagnostics |
| Quadruples | `chali emit app.chl out build/app` | Quadruples, data and layout |
| Bytecode | `chali svmb app.chl out build/app` | VM program and companion data |
| Embed | `stackvm::Machine` | Shared VM on desktop or embedded Linux |

Quadruples and VM execution use the highest supported optimization defaults. Reflection, threads and embedded Linux budgets depend on actual host builds. The compiler embeds its standard library; external drivers and databases remain deployment dependencies.

See [toolchain](09-toolchain.md), [deployment](06-deployment.md), [host integration](08-host-integration.md) and [capabilities](17-capabilities.md).
