# Magel for VS Code

Magel（`.mgl`）的编辑器支持：**语言服务器就是编译器本体**（`magel lsp`），所以只用
一个可执行文件 —— 不需要另外编 `library/lsp` 的 `lsp-server`，开发工具和语言服务器原生内建，无需安装 Python。

## 能力

| 能力 | 说明 |
| --- | --- |
| 诊断 | 真实前端检查（`magel --check --diagnostics=json`）：语法错误带列、语义错误覆盖整行；码是编译器自己的稳定错误码 |
| 悬停 | 函数、工程类型与字段声明、重载及文档注释；auto 变量及 auto 关键字显示完整推断类型、泛型实参与高阶 fn 参数/返回层次 |
| 参数提示 | `(` / `,` 及宏的 `<` / `[` / `{` 触发；显示泛型、默认参数、重载与当前参数；多括号宏按组高亮、统计独立尾参 |
| 类型内联提示 | 显示 auto 变量、for-in 元素和上下文匿名函数参数的推断类型，可关闭 |
| 引用与重命名 | 按原始绑定区分同名遮蔽，支持跨模块和未保存文档；歧义、冲突及无法确认的宏模板引用拒绝重命名，应用前检查文档版本 |
| 关联诊断 | 移动、借用及返回表达式清理冲突可关联原始声明、显式捕获或 defer 位置 |
| 宏展开预览 | 复用真实预处理器，展开行可导航回源文件；整数槽生成名显示对应关系并跳回原始槽位 |
| 宏体独立高亮 | `$macro` 到 `$end_macro` 使用独立 TextMate 区域；指令、形参、`$for` 循环变量和整数临时槽分别着色，字符串与注释保持原有角色；语义着色同步区分声明和使用 |
| 工程符号 | 在工作区其他 `.mgl` 文件中搜索函数、方法、结构体、枚举、接口和字段 |
| 补全 | 当前作用域、标准库、工程模块和成员（`:` / `.` 触发）；选择尚未导入模块的函数时补入 `import` |
| 语义着色 | 区分类型、泛型参数、变量、参数、函数和成员；颜色由当前主题决定 |
| 跳转定义 | 名字引用 → 当前文件或导入模块的声明处 |
| 文档大纲 | 当前文档显式声明的名字，区分函数、变量和类型 |
| 格式化 | 保守：只规范缩进与行尾空白，注释 / 字符串 / 宏指令逐字保留 |
| 折叠 | `$macro` / `$for` 宏块成对折叠（客户端计算，不需要服务器） |
| 运行 | 标题栏 ▶ / 右键 / 命令面板跑 `magel run <当前文件>`：先存盘，再在集成终端里执行，工作目录是文件所在目录，标准输入可直接交互 |
| 断点调试 | 运行与调试面板里选「Magel」，在 `.mgl` 上打断点即可：编译器本体就是调试适配器（`magel --dap <文件>`），支持断点、继续、步进 / 步过 / 步出与调用栈；程序输出显示在调试控制台 |

## 内嵌的编译器

首次开发或打包插件，先在 `vscode/` 执行 `npm ci` 安装锁定的测试依赖。`npm test` 和 `npm run package` 都运行真实 TextMate/Oniguruma 引擎的宏体高亮回归；测试依赖不进入安装包。

插件包自带编译器：`bin/magel[.exe]` —— 打包前由 `node tools/embed-compiler.js` 放进去，
`vsce package` 会自动跑这一步（`vscode:prepublish`）。装完即用：不需要用户先构建工具链，
语言服务器和依赖工具均由原生 C++ 实现，无需安装 Python。

- 刷新内嵌副本：在项目根目录 `make` 出 `./magel[.exe]`，然后在 `vscode/` 里 `npm run embed`；
- 查找顺序：设置 `magel.path` → 内嵌 `bin/` → 工作区（`build/work/magel/magel`、`magel/magel`…）→ PATH；
  输出面板会打印实际用的是哪一份。

> `bin/` 不进版本库（7 MB 二进制），只在打包时生成。

## 设置

| 设置 | 说明 |
| --- | --- |
| `magel.path` | `magel` 编译器路径；留空时依次用**插件自带的 `bin/magel`**、`build/work/magel/magel`、`build/magel/magel`、`magel/magel`、`PATH`（Windows 带 `.exe`） |
| `magel.traceServer` | 设为 `messages` 把与 `magel lsp` 的协议消息写进输出面板 |
| `magel.inlayHints.enabled` | 是否显示推断类型内联提示，默认开启；也受 VS Code 的内联提示设置控制 |

## 命令

- **Magel: 重启语言服务器**
- **Magel: 显示语言服务器日志**
- **Magel: 打开语言文档**（打开 `magel/doc/02-language-tour.md`）
- **Magel: 运行当前文件**（标题栏的 ▶ 与右键菜单里的同一项）

## 打包

```sh
npm run package            # 内嵌编译器 → 清单自检 → 冒烟测试 → 产出 dist/*.vsix
npm run package -- --skip-tests   # 跳过自检，只打包
```

`tools/package.js` 会按顺序做四件事：`tools/embed-compiler.js` 把编译器放进 `bin/`、
`tools/check-manifest.js` 自检清单、`node test/smoke.js` 跑 23 项冒烟（起真 `magel lsp`），
最后调 `vsce package` 产出 `dist/magel-editor-<版本>.vsix`。`vsce` 优先用 PATH 上的那份，
没有就退回 `npx --yes @vscode/vsce`；加 `--dry-run` 可以只打印要跑的命令。

## 本地自检

```sh
node magel/vscode/test/smoke.js                 # 折叠 + LSP 往返（自动找 magel）
node magel/vscode/test/smoke.js --magel <路径>  # 指定编译器
node magel/vscode/tools/check-manifest.js       # 清单自检（语言 / 高亮 / 图标注册齐了没）
```

## 作者与发布者（清单字段）

| 字段 | 值 | 说明 |
| --- | --- | --- |
| `publisher` | `rabbitc999` | 发布者**标识**：必须是 ASCII，且与 marketplace 上的 publisher 账号一致。插件全名是 `<发布者>.<名字>`（本插件即 `rabbitc999.magel-editor`）——换标识等于换身份，装过旧标识的要卸掉重装。 |
| `author` | 小兔糖糖OvO `<rabbitc999@outlook.com>` | 详情页里显示的**作者**（人类可读名）。`publisher` 不收中文（vsce 会报 *Expected the identifier of a publisher, not its human-friendly name*），所以名字写在这里。 |
| `license` | `MIT` | 许可见 项目根的 `LICENSE`。 |
| `icon` | `icon.png` | 插件图标（魔术棒 + `C` + `Value`，由项目根的 `tools/make_icon.py` 生成）。 |

发布：先在 marketplace 建好 `rabbitc999` 这个 publisher 并配好 PAT，然后 `npx @vscode/vsce publish`。


## 在线模块依赖

在 `magel.toml` 的右键菜单或命令面板中执行 **Magel: 下载并锁定模块依赖**、**Magel: 离线校验模块依赖** 和 **Magel: 打开在线依赖缓存** 命令。安装调用 `magel deps lock` 并递归下载到编译器旁的 `libraries/`，生成 `magel.lock` 后自动刷新语言服务器。多工程会提供选择；详细日志见依赖输出通道。下载只在受信任的工作区执行。缓存不会打入插件包。

补全提示保留完整名称以说明来源；没有名称冲突时插入简写，有跨模块同名或当前作用域遮蔽时保留限定名，同模块函数重载不视为冲突。

工程类型和字段提示会递归读取导入模块，并优先使用已打开的未保存内容。成员提示目前以显式声明的接收者类型为依据，复杂表达式的返回类型推断仍有局限。插件未提供完整的工程级重命名与查找引用功能。

## 宏提示与展开预览

普通语言服务器同时提供 `$macro` / `$replace` 定义的补全、参数提示、文档悬停和定义跳转，索引 `%include` 引入的宏。当前文件的 `%define` 符号也进入补全。命令面板或源码右键选择 **Magel: 预览宏展开**，在旁边打开只读的展开结果。预览使用未保存的当前源码及已打开的包含文件，调用编译器相同的预处理器和宏展开器；遇到错误会显示诊断，不生成伪展开结果。修改后再次运行命令即可刷新预览。
