# 25 执行方式与交付边界

当前只提供 VM 执行。`run` 编译源码并进入 VM；`emit` 导出四元式及数据；`svmb` 导出供 VM 加载的字节码。

| 场景 | 入口 | 结果 |
| --- | --- | --- |
| 直接运行 | `magel run app.mgl` | VM 执行及退出状态 |
| 检查 | `magel check app.mgl` | 语法、类型、借用与语义诊断 |
| 四元式 | `magel emit app.mgl out build/app` | `.quad`、数据区及布局 |
| 字节码 | `magel svmb app.mgl out build/app` | VM 程序及配套数据 |
| 宿主嵌入 | `stackvm::Machine` | 同一 VM 在桌面或板端执行 |

四元式与 VM 默认启用最高可用优化。反射、线程及板端预算以实际构建宏和目标宿主为准。
标准库内嵌在编译器中，外部数据库、动态库及外设仍由部署环境提供。

详见[工具链](09-toolchain.md)、[交付部署](06-deployment.md)、[宿主集成](08-host-integration.md)与[当前能力](17-capabilities.md)。
