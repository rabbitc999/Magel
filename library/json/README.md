# Magel 内置 JSON 库副本

`json.hpp`、`json.cpp`、`bind.hpp` 和测试从 `library/json` 复制，解析与序列化语义以该库为准。
Magel 通过 `std::json` 与 `source/glue/std_json.hpp` 调用；源码直接编入胶水翻译单元，不单独链接对象文件。
更新上游时应显式同步副本并运行库测试和 Magel 跨执行方式测试。原始库使用 C++ 标准容器分配。
Magel 接口与生命周期见 [`doc/library/std-json.md`](../doc/library/std-json.md)。
