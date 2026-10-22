#include <cstdint>

/**
 * @brief 在独立宿主胶水模块中绑定集合编解码及同步回调回归入口。
 * @param registry 当前编译器加载的胶水注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有接口绑定成功时返回 true。
 */
bool bindChaLiGlue(chali_glue::Registry &registry, Memory::String *error)
{
    using Ints = Memory::Vector<std::int64_t>;
    using IntMap = Memory::Map<std::int64_t, std::int64_t>;
    using IntSet = Memory::Set<std::int64_t>;
    using Transform = chali_glue::Function<std::int64_t(std::int64_t)>;
    return registry.bindTyped<Ints, Ints, Transform>("collectionTransform", [](Ints values, Transform transform) {
        for (auto &value : values) value = transform(value);
        return values; }, error) && registry.bindTyped<IntMap, IntMap>("collectionMapCopy", [](IntMap values) { return values; }, error) && registry.bindTyped<IntSet, IntSet>("collectionSetCopy", [](IntSet values) { return values; }, error);
}
