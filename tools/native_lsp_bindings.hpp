/**
 * @brief 在不可变源码及可见类型集合上缓存绑定结果，供所有编辑能力共用。
 * @param text 当前快照的源码。
 * @param types 该请求可见的类型名。
 * @return 按声明位置排序的绑定副本，身份由文档 URI 与 offset 决定。
 * @note 缓存受互斥锁保护，最多保留十六份、总计十六 MiB；源码相等检查防止指纹碰撞。
 */
std::vector<Binding> bindings(const std::string &text, std::set<std::string> types)
{
    struct Entry
    {
        std::string text;
        std::set<std::string> types;
        std::vector<Binding> bindings;
        size_t bytes;
    };
    static std::mutex mutex;
    static std::map<std::string, Entry> cache;
    static size_t bytes = 0;
    std::string key = lspHash(text);
    for (const auto &type : types) key += ":" + type;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto found = cache.find(key);
        if (found != cache.end() && found->second.text == text && found->second.types == types) return found->second.bindings;
    }
    auto result = scanBindings(text, types);
    size_t size = text.size() + key.size() + result.capacity() * sizeof(Binding);
    for (const auto &binding : result) size += binding.name.capacity() + binding.type.capacity() + binding.kind.capacity() + binding.iterable.capacity() + binding.scope.capacity() * sizeof(int);
    for (const auto &type : types) size += type.size() + sizeof(std::string);
    if (size <= 1024 * 1024)
    {
        std::lock_guard<std::mutex> lock(mutex);
        while (!cache.empty() && (cache.size() >= 16 || bytes + size > 16 * 1024 * 1024))
        {
            bytes -= cache.begin()->second.bytes;
            cache.erase(cache.begin());
        }
        auto old = cache.find(key);
        if (old != cache.end())
        {
            bytes -= old->second.bytes;
            cache.erase(old);
        }
        cache.emplace(key, Entry{ text, std::move(types), result, size });
        bytes += size;
    }
    return result;
}
