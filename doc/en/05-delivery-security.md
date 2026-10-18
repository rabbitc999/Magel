# 05 Delivery security

Magel can deliver ordinary bytecode, compact bytecode with unnamed symbols, or an AOT executable. Choose a runtime form, retain diagnostic mappings, and account for executable logic remaining available for analysis.

Bytecode encryption has been removed. `--encrypt` and `[magel] encrypt` are no longer supported. Rebuild historical encrypted artifacts from source.

## Delivery forms

| Artifact | Recipient needs | Typical use |
| --- | --- | --- |
| `.mgl` source | Magel compiler | Source sharing and joint development |
| Ordinary `.svmb` | StackVM host with matching capabilities | Device loading and portable deployment |
| Compact `.svmb` | StackVM host supporting compact symbols | Smaller files and removal of readable symbol names |
| AOT executable | Matching platform environment | Standalone native deployment |

A `.svmb` file requires a VM or embedded host. AOT exports the program as a native executable.

## Export and distribute

Ordinary bytecode:

```sh
magel svmb src/main.mgl out build/app
```

Compact bytecode:

```sh
magel svmb src/main.mgl out build/app --compress-symbols
```

The compact command generates `build/app.svmb` and `build/app.symbol_map.json`. Keep the mapping on the development side; use `--symbol-map FILE` to select its path. Running the program does not require the mapping.

AOT executable:

```sh
magel aot src/main.mgl out build/app
```

Distribute the executable for its target platform; Windows artifacts use the `.exe` suffix. See [Deployment](06-deployment.md) for execution forms and runtime requirements.

## Boundaries

Symbol compression removes readable symbol names and reduces size while retaining instructions, constants, and executable logic. String literals can still reveal business information. Recipients can analyze bytecode behavior, and AOT executables can be disassembled.

The container checksum detects accidental corruption and does not authenticate a publisher. Deployment systems and hosts are responsible for release provenance and access controls.

Source locations are omitted by default; request debug records with `--debug`. Preserve the symbol mapping corresponding to each deployed build for offline diagnosis.

## Artifact management

| File | Kept by | Purpose |
| --- | --- | --- |
| `.mgl` | Developer or collaborators | Source and rebuilds |
| `.svmb` | Runtime recipient | VM loading |
| `.symbol_map.json` | Developer | Offline diagnosis |
| AOT executable | Runtime recipient | Native execution |
| Debug builds and build records | Developer | Reproduction and troubleshooting |

Rebuild from source when upgrading and verify that the target VM supports the required program capabilities. The loader rejects historical encrypted flags, and the project parser rejects the removed configuration key.

## Next

- [Toolchain](09-toolchain.md): commands and builds.
- [Binary format](../15-svmb-format.md): container validation.
- [Compact symbols](../16-compact-symbols.md): symbol compression and mapping.
- [Panic traces](../18-panic-trace.md): diagnostics and location mapping.
