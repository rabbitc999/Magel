# 当前任务（0.25.1）

## 任务一：修复 `?` 传播在特定局部变量名下丢失析构导致的内存泄漏

复现样例（`Result<int32,int32>` 连续两次 `?` 传播）：

```magel
Result<int32, int32> next(Result<int32, int32> a)
{
    int32 val = a?;
    val += 1;
    if (val == 38) {
        return Err(12);
    }
    return Ok(val);
}

Result<int32, int32> ff(Result<int32, int32> a)
{
    int32 t1 = next(a)?;   // 变量名换成 b 就不泄漏
    return Ok(t1);
}

void main()
{
    let a = Result<int32, int32>::Ok(0);
    match (ff(a)) {
        Ok(t) => { println("OK={}", t); }
        Err(t) => { println("ERR={}", t); }
    }
}
```

- [ ] 定位根因：`library/simple_ast/simple_ast.cpp` 的 codegen 临时名是 `"t" + 序号`，与用户局部变量 `t1` 撞名，生成的 `?` 传播临时槽被用户变量覆盖，源聚合的析构登记随之丢失。
- [ ] 修复：codegen 内部临时名改为 `__temp_t<n>`（用户建议），并与程序里出现的名字做冲突检查，撞名就继续递增。
- [ ] 回归样例进 `example/samples/`（含期望输出）与 `tests/`，确认不再出现「程序结束时堆上还有 … 没有 drop」提示。
- [ ] 版本基线升到 `0.25.1`（`VERSION`、`magel_package.hpp`、`source/lsp/*`、`tools/mageldev.py`、`vscode/package.json`、`AGENTS.md`），`tools/changelog_data.json` 增补条目并 `--build`。
- [ ] 提交并推送到 `origin master`。

## 任务二：0.25.1 口径调整——取消「交付字节码」承诺

- [ ] 主线口径改为「一次编写，处处运行」（对标 Java 的 Write Once, Run Anywhere），不再把「交付字节码」当承诺。
- [ ] 弱化嵌入式交付描述：可以说明具备该能力，但不作为交付方式宣称。
- [ ] `doc/book` 的「交付与部署」改为「部署字节码」。
- [ ] 删除所有「交付字节码」的描述（中英文同步，含 README、doc、doc/book、vscode 文档）。
- [ ] 简单验证（构建 + 相关文档门禁），不跑全量 `ci.sh`。
- [ ] 提交。

## 任务三：收尾

- [ ] 全部做完后删除本文件。
- [ ] 在 `doc/` 下写语法冻结清单（与 `doc/21-semantics-frozen.md` 配套）。
