/**
 * @brief 一份不可变源码的词法、声明与作用域索引。
 */
struct LspUnit
{
    std::shared_ptr<const std::string> text;
    PositionIndex positions;
    std::vector<Token> tokens, raw;
    std::vector<Record> records;
    std::vector<std::string> imports, includes;
    std::vector<size_t> braces;
    std::vector<std::vector<int>> scopes;
    std::vector<std::pair<size_t, size_t>> macroRegions;
    /**
     * @brief 构建词法与作用域索引。
     * @param source 原文。
     */
    explicit LspUnit(std::string source) : text(std::make_shared<const std::string>(std::move(source))),
                                           positions(*text), tokens(lex(*text)), raw(lex(*text, true))
    {
        imports = directives(*text, "import");
        std::vector<int> scope{ 0 };
        int serial = 0;
        size_t macroBegin = std::string::npos;
        for (const auto &token : tokens)
        {
            if (token.word == "$macro") macroBegin = token.a;
            if (token.word == "$end_macro" && macroBegin != std::string::npos)
            {
                macroRegions.push_back({ macroBegin, token.b });
                macroBegin = std::string::npos;
            }
            if (token.word == "{")
                scope.push_back(++serial);
            else if (token.word == "}" && scope.size() > 1)
                scope.pop_back();
            else
                continue;
            braces.push_back(token.a);
            scopes.push_back(scope);
        }
        if (macroBegin != std::string::npos) macroRegions.push_back({ macroBegin, text->size() });
        for (size_t at = 0; at + 2 < raw.size(); ++at)
            if (raw[at].word == "%" && (raw[at + 1].word == "include" || raw[at + 1].word == "include_once" || raw[at + 1].word == "import") && raw[at + 2].kind == 9)
                includes.push_back(raw[at + 2].word.substr(1, raw[at + 2].word.size() - 2));
    }
    /**
     * @brief 估算源码、词法与声明所占内存，供缓存淘汰使用。
     * @return 估计字节数。
     */
    size_t bytes() const
    {
        size_t total = sizeof(*this) + text->capacity() + positions.lines.capacity() * sizeof(size_t);
        total += macroRegions.capacity() * sizeof(std::pair<size_t, size_t>);
        for (const auto *list : { &tokens, &raw })
        {
            total += list->capacity() * sizeof(Token);
            for (const auto &token : *list) total += token.word.capacity();
        }
        total += records.capacity() * sizeof(Record) + braces.capacity() * sizeof(size_t) + scopes.capacity() * sizeof(std::vector<int>);
        for (const auto &scope : scopes) total += scope.capacity() * sizeof(int);
        for (const auto &r : records)
        {
            for (const auto *value : { &r.name, &r.module, &r.owner, &r.qualified, &r.signature, &r.documentation, &r.source, &r.type, &r.kind }) total += value->capacity();
            for (const auto &p : r.parameters) total += p.dump().size();
        }
        for (const auto *list : { &imports, &includes })
        {
            total += list->capacity() * sizeof(std::string);
            for (const auto &value : *list) total += value.capacity();
        }
        return total;
    }
};
thread_local const LspUnit *activeUnit = nullptr;
/**
 * @brief 将不可变索引绑定到当前工作线程，退出时恢复旧上下文。
 */
struct LspIndexScope
{
    const LspUnit *previous;
    const PositionIndex *positions;
    /**
     * @brief 绑定索引。
     * @param unit 当前版本。
     */
    explicit LspIndexScope(const LspUnit &unit) : previous(activeUnit), positions(activePositions)
    {
        activeUnit = &unit;
        activePositions = &unit.positions;
    }
    /**
     * @brief 恢复线程上下文。
     */
    ~LspIndexScope()
    {
        activeUnit = previous;
        activePositions = positions;
    }
};
/**
 * @brief 缓存内容指纹。
 * @param value 文本。
 * @return 稳定十六进制键。
 */
std::string lspHash(const std::string &value)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : value) hash = (hash ^ c) * 1099511628211ULL;
    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
}
/**
 * @brief LSP 可复用的文件与声明缓存；缓存损坏或不可写时退回内存分析。
 */
class LspCache
{
    struct DiskText
    {
        fs::file_time_type modified;
        uintmax_t size;
        std::string body;
        uint64_t used;
    };
    struct UnitEntry
    {
        std::shared_future<std::shared_ptr<const LspUnit>> future;
        size_t bytes = 0;
        uint64_t used = 0;
    };
    std::mutex mutex_, diskMutex_;
    std::map<std::string, UnitEntry> units_;
    std::map<std::string, DiskText> disk_;
    std::map<std::string, std::set<std::string>> dependencies_;
    size_t unitBytes_ = 0, diskBytes_ = 0;
    uint64_t unitClock_ = 0, diskClock_ = 0;
    std::string stamp_;
    J record(const Record &r) const
    {
        return object({ { "name", r.name }, { "module", r.module }, { "owner", r.owner }, { "qualified", r.qualified }, { "signature", r.signature }, { "documentation", r.documentation }, { "source", r.source }, { "type", r.type }, { "kind", r.kind }, { "offset", int64_t(r.offset) }, { "parameters", r.parameters } });
    }

public:
    fs::path directory;
    std::atomic<uint64_t> scans{ 0 }, hits{ 0 }, restored{ 0 }, checks{ 0 }, diagnosticHits{ 0 }, cancelled{ 0 }, removed{ 0 };
    /**
     * @brief 选择程序目录的 build/lsp，并按编译版本隔离记录。
     * @param compiler 编译器路径。
     */
    explicit LspCache(const fs::path &compiler) : directory(compiler.parent_path() / "build" / "lsp")
    {
        std::error_code ec;
        const auto modified = fs::last_write_time(compiler, ec).time_since_epoch().count();
        stamp_ = lspHash(compiler.u8string() + std::to_string(modified) + __DATE__ + __TIME__);
    }
    /**
     * @brief 原子写入缓存；写入失败不影响提示。
     * @param path 目标文件。
     * @param value JSON 内容。
     */
    void save(const fs::path &path, const J &value) const
    {
        try
        {
            magel_incremental::replaceCacheFile(path, [&](std::ostream &out) { out << value.dump(); });
        } catch (...) {}
    }
    /**
     * @brief 成功复用时至多每小时刷新访问时间，避免活跃缓存因七天期限被清理。
     * @param file 缓存文件。
     */
    void touch(const fs::path &file) const
    {
        std::error_code ec;
        const auto modified = fs::last_write_time(file, ec);
        const auto now = fs::file_time_type::clock::now();
        if (!ec && now - modified > std::chrono::hours(1)) fs::last_write_time(file, now, ec);
    }
    /**
     * @brief 缓存文件键。
     * @param kind 缓存类别。
     * @param source 文档 URI。
     * @return 稳定文件位置。
     */
    fs::path path(const std::string &kind, const std::string &source) const { return directory / kind / (stamp_ + "-" + lspHash(source) + ".json"); }
    /**
     * @brief 读取文件，仅在大小或修改时间变化时重新加载。
     * @param file 文件。
     * @return UTF-8 源码。
     */
    std::string source(const fs::path &file)
    {
        std::lock_guard<std::mutex> lock(diskMutex_);
        const auto name = normalizedPath(file).u8string();
        const auto time = fs::last_write_time(file);
        const auto size = fs::file_size(file);
        auto found = disk_.find(name);
        if (found != disk_.end() && found->second.modified == time && found->second.size == size)
        {
            found->second.used = ++diskClock_;
            return found->second.body;
        }
        const auto body = read(file);
        if (found != disk_.end()) diskBytes_ -= found->second.body.capacity();
        disk_[name] = { time, size, body, ++diskClock_ };
        diskBytes_ += disk_[name].body.capacity();
        while (disk_.size() > 1 && (disk_.size() > 512 || diskBytes_ > 32ULL * 1024 * 1024))
        {
            auto oldest = disk_.end();
            for (auto it = disk_.begin(); it != disk_.end(); ++it)
                if (it->first != name && (oldest == disk_.end() || it->second.used < oldest->second.used)) oldest = it;
            if (oldest == disk_.end()) break;
            diskBytes_ -= oldest->second.body.capacity();
            disk_.erase(oldest);
        }
        return body;
    }
    /**
     * @brief 文件监视通知使磁盘读取缓存失效。
     * @param file 已修改文件。
     */
    void invalidate(const fs::path &file)
    {
        std::lock_guard<std::mutex> lock(diskMutex_);
        auto found = disk_.find(normalizedPath(file).u8string());
        if (found != disk_.end())
        {
            diskBytes_ -= found->second.body.capacity();
            disk_.erase(found);
        }
    }
    /**
     * @brief 登记模块和包含文件依赖。
     * @param root 文档 URI。
     * @param dependencies 依赖 URI。
     */
    void dependencies(const std::string &root, const std::set<std::string> &dependencies)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dependencies_[root].insert(dependencies.begin(), dependencies.end());
    }
    /**
     * @brief 关闭入口文件时释放其依赖记录。
     * @param root 文档 URI。
     */
    void forget(const std::string &root)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dependencies_.erase(root);
    }
    /**
     * @brief 返回需要重新诊断的依赖者。
     * @param changed 修改的 URI。
     * @return 依赖文档。
     */
    std::vector<std::string> dependents(const std::string &changed)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto &entry : dependencies_)
            if (entry.first != changed && entry.second.count(changed)) result.push_back(entry.first);
        return result;
    }
    /**
     * @brief 同一源码版本只构建一次索引，其他提示共享结果。
     * @param uri 文档身份。
     * @param body 当前源码。
     * @return 不可变索引。
     */
    std::shared_ptr<const LspUnit> unit(const std::string &uri, const std::string &body)
    {
        const auto hash = lspHash(body), key = uri + ":" + hash;
        std::shared_future<std::shared_ptr<const LspUnit>> future;
        std::shared_ptr<std::promise<std::shared_ptr<const LspUnit>>> promise;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto found = units_.find(key);
            if (found != units_.end())
            {
                ++hits;
                found->second.used = ++unitClock_;
                future = found->second.future;
            }
            else
            {
                promise = std::make_shared<std::promise<std::shared_ptr<const LspUnit>>>();
                future = promise->get_future().share();
                units_[key] = { future, 0, ++unitClock_ };
            }
        }
        if (!promise) return future.get();
        try
        {
            auto result = std::make_shared<LspUnit>(body);
            bool loaded = false;
            try
            {
                const auto data = parse(read(path("index", uri)));
                if (integer(get(data, "schema")) == 1 && str(data, "hash") == hash && str(data, "uri") == uri && get(data, "records").asArray())
                {
                    for (const auto &entry : array(get(data, "records")))
                    {
                        Record r;
                        r.name = str(entry, "name");
                        r.module = str(entry, "module");
                        r.owner = str(entry, "owner");
                        r.qualified = str(entry, "qualified");
                        r.signature = str(entry, "signature");
                        r.documentation = str(entry, "documentation");
                        r.source = uri;
                        r.type = str(entry, "type");
                        r.kind = str(entry, "kind");
                        r.offset = size_t(get(entry, "offset").intOr(-1));
                        r.parameters = array(get(entry, "parameters"));
                        if (r.offset > body.size() || r.name.empty()) throw std::runtime_error("声明缓存损坏");
                        result->records.push_back(std::move(r));
                    }
                    loaded = true;
                    ++restored;
                    touch(path("index", uri));
                }
            } catch (...) { result->records.clear(); }
            if (!loaded)
            {
                result->records = scan(uri, body);
                ++scans;
                A records;
                for (const auto &r : result->records) records.push_back(record(r));
                save(path("index", uri), object({ { "schema", 1 }, { "hash", hash }, { "uri", uri }, { "records", records } }));
            }
            const auto bytes = result->bytes();
            promise->set_value(result);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto found = units_.find(key);
                if (found != units_.end())
                {
                    found->second.bytes = bytes;
                    unitBytes_ += bytes;
                }
                while (units_.size() > 1 && (units_.size() > 256 || unitBytes_ > 64ULL * 1024 * 1024))
                {
                    auto oldest = units_.end();
                    for (auto it = units_.begin(); it != units_.end(); ++it)
                        if (it->first != key && it->second.bytes && (oldest == units_.end() || it->second.used < oldest->second.used)) oldest = it;
                    if (oldest == units_.end()) break;
                    unitBytes_ -= oldest->second.bytes;
                    units_.erase(oldest);
                }
            }
            return result;
        } catch (...)
        {
            promise->set_exception(std::current_exception());
            throw;
        }
    }
    /**
     * @brief 自动清理过期缓存，并将超出 256 MiB 的缓存压到 192 MiB；只删除 build/lsp 内的普通文件。
     */
    void cleanup()
    {
        try
        {
            std::error_code ec;
            if (!fs::is_directory(directory, ec) || fs::is_symlink(directory, ec)) return;
            struct File
            {
                fs::path path;
                fs::file_time_type modified;
                uintmax_t size;
            };
            std::vector<File> files;
            uintmax_t size = 0;
            const auto now = fs::file_time_type::clock::now();
            for (fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                if (it->is_symlink(ec))
                {
                    it.disable_recursion_pending();
                    continue;
                }
                if (!it->is_regular_file(ec)) continue;
                const auto path = it->path().lexically_normal(), relative = path.lexically_relative(directory.lexically_normal());
                if (relative.empty() || *relative.begin() == "..") continue;
                const auto time = it->last_write_time(ec);
                if (ec) continue;
                const auto bytes = it->file_size(ec);
                if (ec) continue;
                if (now - time > std::chrono::hours(24 * 7))
                {
                    if (fs::remove(path, ec)) ++removed;
                    continue;
                }
                files.push_back({ path, time, bytes });
                size += bytes;
            }
            if (size > 256ULL * 1024 * 1024)
            {
                std::sort(files.begin(), files.end(), [](const File &a, const File &b) { return a.modified < b.modified; });
                for (const auto &file : files)
                {
                    if (size <= 192ULL * 1024 * 1024) break;
                    if (now - file.modified < std::chrono::minutes(10)) continue;
                    if (fs::remove(file.path, ec))
                    {
                        size -= file.size;
                        ++removed;
                    }
                }
            }
        } catch (...) {}
    }
    /**
     * @brief 返回用于诊断性能回归的计数。
     * @return 机器可读统计。
     */
    J stats()
    {
        std::scoped_lock lock(mutex_, diskMutex_);
        return object({ { "scans", int64_t(scans.load()) }, { "indexHits", int64_t(hits.load()) }, { "restored", int64_t(restored.load()) }, { "checks", int64_t(checks.load()) }, { "diagnosticHits", int64_t(diagnosticHits.load()) }, { "cancelled", int64_t(cancelled.load()) }, { "removed", int64_t(removed.load()) }, { "cacheDirectory", directory.u8string() }, { "indexBytes", int64_t(unitBytes_) }, { "sourceBytes", int64_t(diskBytes_) }, { "indexEntries", int64_t(units_.size()) } });
    }
};
