# 编译期用户注解样例

`annotations.mgl` 展示注解声明、默认值、位置与命名实参，以及接口、结构体、枚举、字段、静态字段、构造函数和方法上的注解。函数与成员使用 Magel 现行 C 风格语法。

先按 [注解与反射文档](../../doc/19-reflection.md) 开启编译器和 VM 的两项构建宏，再从仓库根执行：

```sh
magel/magel --reflection magel/example/reflection/annotations.mgl --out build/annotation-example
```

预期输出：

```text
annotations 42 7
```

本阶段注解保存在编译器 AST 中并完成类型检查，尚不能在 Magel 运行时查询或调用。`@Reflect` 内建暴露与 `std::reflect::` API 属于后续模块；示例中的方法调用都是普通静态检查后的调用。

运行专项验证：

```sh
python tests/magel_annotations.py
```

脚本还会生成并实际运行 VM 和 AOT 两种独立程序。
