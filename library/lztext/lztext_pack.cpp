/**
 * @file lztext_pack.cpp
 *
 * @brief lztext 的命令行打包工具（构建期用）。
 *
 *  用法：
 *    lztext_pack  <输入文件> <输出文件>    压缩；写完立刻解压回读比对，不一致就报错退出
 *    lztext_pack -d <输入文件> <输出文件>  解压
 *
 *  内嵌脚本（Python）不需要自己实现压缩算法，直接调用本工具即可，
 *  保证"压缩/解压只有一份 C++ 实现"这一条成立。
 */

#include "lztext.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

/**
 * @brief 将文件内容读取到输出字符串。
 * @param path 目标文件或目录的路径。
 * @param out 接收处理结果的输出对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool readFile(const std::string &path, std::string &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

/**
 * @brief 将字符串内容写入指定文件。
 * @param path 目标文件或目录的路径。
 * @param data 输入或待写入的字节数据。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool writeFile(const std::string &path, const std::string &data)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(data.data(), (std::streamsize)data.size());
    return out.good();
}

}  // namespace

/**
 * @brief 运行本模块的命令行示例程序。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main(int argc, char **argv)
{
    int argi = 1;
    bool decompressMode = false;
    if (argc > 1 && std::string(argv[1]) == "-d")
    {
        decompressMode = true;
        ++argi;
    }
    if (argi + 1 >= argc)
    {
        std::cerr << "用法：lztext_pack [-d] <输入文件> <输出文件>\n";
        return 2;
    }
    const std::string inPath = argv[argi];
    const std::string outPath = argv[argi + 1];

    std::string input;
    if (!readFile(inPath, input))
    {
        std::cerr << "打不开输入文件：" << inPath << "\n";
        return 1;
    }

    if (decompressMode)
    {
        std::string plain;
        if (!lztext::decompress(reinterpret_cast<const unsigned char *>(input.data()), input.size(), plain))
        {
            std::cerr << "解压失败：" << inPath << "\n";
            return 1;
        }
        if (!writeFile(outPath, plain))
        {
            std::cerr << "写文件失败：" << outPath << "\n";
            return 1;
        }
        std::printf("解压 %zu -> %zu 字节\n", input.size(), plain.size());
        return 0;
    }

    const std::vector<unsigned char> packed = lztext::compress(input, 9);

    // 自检：压缩完立刻解压回读，确保产物一定能被解开。
    std::string back;
    if (!lztext::decompress(packed.data(), packed.size(), back) || back != input)
    {
        std::cerr << "压缩自检失败（往返不一致）：" << inPath << "\n";
        return 1;
    }

    const std::string blob(reinterpret_cast<const char *>(packed.data()), packed.size());
    if (!writeFile(outPath, blob))
    {
        std::cerr << "写文件失败：" << outPath << "\n";
        return 1;
    }
    const double ratio = input.empty() ? 0.0 : (double)packed.size() / (double)input.size();
    std::printf("压缩 %zu -> %zu 字节（比率 %.3f）\n", input.size(), packed.size(), ratio);
    return 0;
}
