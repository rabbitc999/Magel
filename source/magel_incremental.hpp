#include "allocator/magel_allocator.hpp"
#ifndef MAGEL_INCREMENTAL_HPP
#define MAGEL_INCREMENTAL_HPP

#include "preprocess.hpp"

#include <cstdint>
#include <atomic>
#include <chrono>
#include <map>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace magel_incremental
{

namespace fs = std::filesystem;

inline std::uint64_t hashBytes(const Memory::String &value, std::uint64_t hash = 14695981039346656037ull)
{
    for (unsigned char ch : value) hash = (hash ^ ch) * 1099511628211ull;
    return hash;
}

inline Memory::String canonical(const Memory::String &path)
{
    std::error_code ec;
    const fs::path result = fs::weakly_canonical(fs::absolute(path, ec), ec);
    return ec ? fs::path(path).lexically_normal().string() : result.string();
}

inline bool fileHash(const Memory::String &path, std::uint64_t &hash)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    hash = 14695981039346656037ull;
    char buffer[16384];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() != 0)
        for (std::streamsize i = 0; i < file.gcount(); ++i)
            hash = (hash ^ static_cast<unsigned char>(buffer[i])) * 1099511628211ull;
    return !file.bad();
}

inline void writeNumber(std::ostream &out, std::uint64_t number)
{
    for (unsigned i = 0; i < 8; ++i) out.put(static_cast<char>(number >> (i * 8u)));
}

inline bool readNumber(std::istream &in, std::uint64_t &number)
{
    number = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        const int ch = in.get();
        if (ch == EOF) return false;
        number |= static_cast<std::uint64_t>(static_cast<unsigned char>(ch)) << (i * 8u);
    }
    return true;
}

inline void writeString(std::ostream &out, const Memory::String &value)
{
    writeNumber(out, value.size());
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

inline bool readString(std::istream &in, Memory::String &value)
{
    std::uint64_t size = 0;
    if (!readNumber(in, size) || size > 64u * 1024u * 1024u) return false;
    value.resize(static_cast<std::size_t>(size));
    if (size == 0) return true;
    return static_cast<bool>(in.read(&value[0], static_cast<std::streamsize>(size)));
}

inline void writeStrings(std::ostream &out, const Memory::Vector<Memory::String> &values)
{
    writeNumber(out, values.size());
    for (const auto &value : values) writeString(out, value);
}

inline bool readStrings(std::istream &in, Memory::Vector<Memory::String> &values)
{
    std::uint64_t count = 0;
    if (!readNumber(in, count) || count > 1000000u) return false;
    values.resize(static_cast<std::size_t>(count));
    for (auto &value : values)
        if (!readString(in, value)) return false;
    return true;
}

inline bool replaceCacheFile(const fs::path &path, const Memory::Function<void(std::ostream &)> &write)
{
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;
    static std::atomic<std::uint64_t> sequence{ 0 };
#if defined(_WIN32)
    const auto process = _getpid();
#else
    const auto process = getpid();
#endif
    const fs::path temporary = path.string() + ".tmp." + Memory::toString(process) + "." + Memory::toString(std::chrono::steady_clock::now().time_since_epoch().count()) + "." + Memory::toString(sequence.fetch_add(1, std::memory_order_relaxed));
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        write(out);
        out.flush();
        if (!out)
        {
            fs::remove(temporary, ec);
            return false;
        }
    }
    fs::rename(temporary, path, ec);
    if (ec)
    {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(temporary, path, ec);
    }
    if (ec)
    {
        std::error_code cleanup;
        fs::remove(temporary, cleanup);
        return false;
    }
    return true;
}

inline std::uint64_t inputHash(const preprocess::Result &result, std::uint64_t seed);

class Cache
{
public:
    Cache(Memory::String directory, const Memory::String &executable,
          const Memory::Vector<Memory::String> &includes, const Memory::Vector<Memory::String> &defines,
          const Memory::String &projectSignature = Memory::String(),
          const Memory::String &outputOptions = Memory::String()) : directory_(std::move(directory)), includeDirs_(includes)
    {
        std::uint64_t binary = 0;
        if (!fileHash(canonical(executable), binary)) binary = 0;
        // The compile stamp also invalidates caches when argv[0] was resolved via PATH.
        signature_ = hashBytes(Memory::toString(binary) + __DATE__ + __TIME__);
        for (const auto &item : includes) signature_ = hashBytes(canonical(item), signature_);
        for (const auto &item : defines) signature_ = hashBytes(item, signature_);
        if (!projectSignature.empty()) signature_ = hashBytes(projectSignature, signature_);
        outputSignature_ = hashBytes(outputOptions, signature_);
    }

    bool preprocessFile(const Memory::String &path, const preprocess::Options &options,
                        preprocess::Result &result)
    {
        const Memory::String source = canonical(path);
        const fs::path cachePath = fs::path(directory_) / (Memory::toString(hashBytes(source, signature_)) + ".unit");
        if (readUnit(cachePath, source, result))
        {
            ++reused_;
            return true;
        }
        result = preprocess::Result{};
        preprocess::Preprocessor processor(options);
        const auto overlay = overlays_.find(source);
        if (overlay == overlays_.end())
        {
            if (!processor.runFile(path, result)) return false;
        }
        else
        {
            if (!processor.runText(overlay->second, path, fs::path(path).parent_path().string(), result)) return false;
            result.dependencies.push_back(source);
        }
        ++rebuilt_;
        writeUnit(cachePath, source, result);
        return true;
    }

    bool preprocessTextUnit(const Memory::String &path, const preprocess::Options &options,
                            const Memory::String &text, const Memory::String &label,
                            const Memory::String &base, const Memory::String &manifest,
                            preprocess::Result &result)
    {
        const Memory::String source = canonical(path);
        const fs::path cachePath = fs::path(directory_) / (Memory::toString(hashBytes(source + text, signature_)) + ".unit");
        if (readUnit(cachePath, source, result))
        {
            ++reused_;
            return true;
        }
        preprocess::Preprocessor processor(options);
        if (!processor.runText(text, label, base, result)) return false;
        result.dependencies.push_back(manifest);
        ++rebuilt_;
        writeUnit(cachePath, source, result);
        return true;
    }

    /**
     * @brief 设置编辑器的未保存源码，同时用于校验包含依赖的内容指纹。
     * @param values 规范路径到未保存源码的覆盖表。
     */
    void setOverlays(const Memory::Map<Memory::String, Memory::String> &values) { overlays_ = values; }

    std::size_t reused() const { return reused_; }
    std::size_t rebuilt() const { return rebuilt_; }
    /**
     * @brief 返回产物签名，包含影响输出的选项。
     * @return 编译产物签名。
     */
    std::uint64_t signature() const { return outputSignature_; }
    /**
     * @brief 返回本次编译实际读取文件计算哈希的次数。
     * @return 唯一文件读取数。
     */
    std::size_t hashReads() const { return hashReads_; }
    /**
     * @brief 返回本次编译复用文件校验结果的次数。
     * @return 避免重复读取的次数。
     */
    std::size_t hashReuses() const { return hashReuses_; }

    /**
     * @brief 校验增量输出及可选映射文件。
     * @param prefix 输出前缀。
     * @param input 输入哈希。
     * @param binary 是否检查字节码。
     * @param symbolMap PC 映射路径，空值不检查。
     * @return 所有产物匹配返回 true。
     */
    bool outputsCurrent(const Memory::String &prefix, std::uint64_t input, bool binary,
                        const Memory::String &symbolMap = {}) const
    {
        std::ifstream in(fs::path(directory_) / "output.cache", std::ios::binary);
        std::uint64_t magic = 0, storedSignature = 0, storedInput = 0, count = 0;
        if (!readNumber(in, magic) || magic != 0x4d434f5554505554ull || !readNumber(in, storedSignature) || storedSignature != outputSignature_ || !readNumber(in, storedInput) || storedInput != input || !readNumber(in, count)) return false;
        const auto expectedPaths = outputPaths(prefix, binary, symbolMap);
        if (count != expectedPaths.size()) return false;
        for (std::uint64_t i = 0; i < count; ++i)
        {
            Memory::String path;
            std::uint64_t saved = 0, actual = 0;
            if (!readString(in, path) || !readNumber(in, saved) || path != expectedPaths[static_cast<std::size_t>(i)] || !fileHash(path, actual) || saved != actual) return false;
        }
        return true;
    }

    /**
     * @brief 保存增量输出指纹。
     * @param prefix 输出前缀。
     * @param input 输入哈希。
     * @param binary 是否包含字节码。
     * @param symbolMap PC 映射路径，空值不保存。
     */
    void saveOutputs(const Memory::String &prefix, std::uint64_t input, bool binary,
                     const Memory::String &symbolMap = {}) const
    {
        Memory::Vector<Memory::String> paths = outputPaths(prefix, binary, symbolMap);
        Memory::Vector<std::uint64_t> hashes;
        for (const auto &path : paths)
        {
            std::uint64_t hash = 0;
            if (!fileHash(path, hash)) return;
            hashes.push_back(hash);
        }
        replaceCacheFile(fs::path(directory_) / "output.cache", [&](std::ostream &out) {
            writeNumber(out, 0x4d434f5554505554ull);
            writeNumber(out, outputSignature_);
            writeNumber(out, input);
            writeNumber(out, paths.size());
            for (std::size_t i = 0; i < paths.size(); ++i)
            {
                writeString(out, paths[i]);
                writeNumber(out, hashes[i]);
            }
        });
    }

private:
    /**
     * @brief 在一次编译中复用同一依赖的内容哈希。
     * @param path 依赖文件。
     * @param hash 输出指纹。
     * @return 文件读取成功。
     */
    bool dependencyHash(const Memory::String &path, std::uint64_t &hash) const
    {
        const auto key = canonical(path);
        const auto overlay = overlays_.find(key);
        if (overlay != overlays_.end())
        {
            hash = hashBytes(overlay->second);
            ++hashReuses_;
            return true;
        }
        std::error_code ec;
        const auto modified = fs::last_write_time(key, ec);
        if (ec) return false;
        const auto size = fs::file_size(key, ec);
        if (ec) return false;
        auto found = dependencyHashes_.find(key);
        if (found != dependencyHashes_.end() && found->second.modified == modified && found->second.size == size)
        {
            hash = found->second.hash;
            ++hashReuses_;
            return true;
        }
        if (!fileHash(key, hash)) return false;
        dependencyHashes_[key] = { hash, size, modified };
        ++hashReads_;
        return true;
    }

    static Memory::Vector<Memory::String> outputPaths(const Memory::String &prefix, bool binary,
                                                      const Memory::String &symbolMap)
    {
        Memory::Vector<Memory::String> paths;
        for (const char *suffix : { ".quad", ".data.bin", ".layout" })
            paths.push_back(canonical(prefix + suffix));
        std::error_code error;
        if (fs::exists(fs::path(prefix + ".abi.cpp"), error))
            paths.push_back(canonical(prefix + ".abi.cpp"));
        if (binary) paths.push_back(canonical(prefix + ".svmb"));
        if (binary && !symbolMap.empty()) paths.push_back(canonical(symbolMap));
        return paths;
    }

    Memory::Vector<Memory::String> directories(const Memory::Vector<Memory::String> &dependencies) const
    {
        Memory::Set<Memory::String> unique;
        for (const auto &item : dependencies) unique.insert(fs::path(canonical(item)).parent_path().string());
        for (const auto &item : includeDirs_) unique.insert(canonical(item));
        return { unique.begin(), unique.end() };
    }

    static std::uint64_t directoryTime(const Memory::String &path)
    {
        std::error_code ec;
        const auto time = fs::last_write_time(path, ec);
        return ec ? 0 : static_cast<std::uint64_t>(time.time_since_epoch().count());
    }

    bool readUnit(const fs::path &path, const Memory::String &source, preprocess::Result &result) const
    {
        std::ifstream in(path, std::ios::binary);
        std::uint64_t magic = 0, signature = 0, count = 0;
        Memory::String storedSource;
        if (!readNumber(in, magic) || magic != 0x4d43554e49543031ull || !readNumber(in, signature) || signature != signature_ || !readString(in, storedSource) || storedSource != source || !readStrings(in, result.dependencies)) return false;
        bool sourceListed = false;
        for (const auto &dependency : result.dependencies)
            if (canonical(dependency) == source) sourceListed = true;
        if (!sourceListed) return false;
        for (const auto &dependency : result.dependencies)
        {
            std::uint64_t saved = 0, actual = 0;
            if (!readNumber(in, saved) || !dependencyHash(dependency, actual) || saved != actual) return false;
        }
        Memory::Vector<Memory::String> dirs;
        if (!readStrings(in, dirs) || dirs != directories(result.dependencies)) return false;
        for (const auto &dir : dirs)
        {
            std::uint64_t saved = 0;
            if (!readNumber(in, saved) || saved != directoryTime(dir)) return false;
        }
        if (!readString(in, result.text) || !readNumber(in, count) || count > 1000000u) return false;
        result.lineMap.resize(static_cast<std::size_t>(count));
        for (auto &origin : result.lineMap)
        {
            std::uint64_t line = 0;
            if (!readString(in, origin.file) || !readNumber(in, line)) return false;
            origin.line = static_cast<std::size_t>(line);
        }
        std::uint64_t checksum = 0;
        return readNumber(in, checksum) && checksum == inputHash(result, signature_);
    }

    void writeUnit(const fs::path &path, const Memory::String &source,
                   const preprocess::Result &result) const
    {
        const auto dirs = directories(result.dependencies);
        Memory::Vector<std::uint64_t> hashes;
        for (const auto &dependency : result.dependencies)
        {
            std::uint64_t hash = 0;
            if (!dependencyHash(dependency, hash)) return;
            hashes.push_back(hash);
        }
        replaceCacheFile(path, [&](std::ostream &out) {
            writeNumber(out, 0x4d43554e49543031ull);
            writeNumber(out, signature_);
            writeString(out, source);
            writeStrings(out, result.dependencies);
            for (const auto hash : hashes) writeNumber(out, hash);
            writeStrings(out, dirs);
            for (const auto &dir : dirs) writeNumber(out, directoryTime(dir));
            writeString(out, result.text);
            writeNumber(out, result.lineMap.size());
            for (const auto &origin : result.lineMap)
            {
                writeString(out, origin.file);
                writeNumber(out, origin.line);
            }
            writeNumber(out, inputHash(result, signature_));
        });
    }

    Memory::String directory_;
    Memory::Map<Memory::String, Memory::String> overlays_;  ///< 未保存源码，避免复用磁盘上的旧模块。
    Memory::Vector<Memory::String> includeDirs_;
    std::uint64_t signature_ = 0;
    std::uint64_t outputSignature_ = 0;
    /**
     * @brief 单次编译内的依赖校验记录；跨编译仍重新读取内容。
     */
    struct DependencyFingerprint
    {
        std::uint64_t hash;
        std::uintmax_t size;
        fs::file_time_type modified;
    };
    mutable Memory::Map<Memory::String, DependencyFingerprint> dependencyHashes_;
    mutable std::size_t hashReads_ = 0, hashReuses_ = 0;
    std::size_t reused_ = 0;
    std::size_t rebuilt_ = 0;
};

inline std::uint64_t inputHash(const preprocess::Result &result, std::uint64_t seed)
{
    std::uint64_t hash = hashBytes(result.text, seed);
    for (const auto &origin : result.lineMap)
    {
        hash = hashBytes(origin.file, hash);
        hash = hashBytes(Memory::toString(origin.line), hash);
    }
    for (const auto &dependency : result.dependencies) hash = hashBytes(dependency, hash);
    return hash;
}

}  // namespace magel_incremental

#endif
