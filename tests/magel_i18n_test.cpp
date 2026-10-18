/**
 * @file magel_i18n_test.cpp
 * @brief 验证双语静态消息、区域解析、工具选项和应用参数边界。
 */
#include "../source/magel_i18n.hpp"
#include "../source/magel_cli.hpp"
#include <cassert>
#include <iostream>

/**
 * @brief 校验语言检测与格式消息；使用环境隔离的独立进程运行。
 * @return 全部通过返回零。
 */
int main()
{
    using namespace magel_i18n;
    assert(parse("zh-CN") == Language::Zh);
    assert(parse("ZH_tw.UTF-8") == Language::Zh);
    assert(parse("ja_JP.UTF-8") == Language::Auto);
    assert(parse("en-US") == Language::En);
    assert(parse("zhwrong") == Language::Auto);
    for (auto chosen : { Language::Zh, Language::En })
    {
        select(chosen);
        const std::string translated = text("目标已存在，拒绝覆盖");
        assert(!translated.empty());
        if (chosen == Language::En) assert(translated.find("already exists") != std::string::npos);
        assert(std::string(text("用户文本 / 利用者の文字列")) == "用户文本 / 利用者の文字列");
    }
    std::vector<std::string> args{ "magel", "--lang", "en", "help", "--", "--lang", "en" };
    std::string error;
    assert(configure(args, error));
    assert(language() == Language::En);
    assert((args == std::vector<std::string>{ "magel", "help", "--", "--lang", "en" }));
    args = { "magel", "--lang", "ja", "new", "untouched" };
    assert(!configure(args, error));
    assert(error.find("ja") != std::string::npos);
    args = { "magel", "--lang" };
    assert(!configure(args, error));
    args = { "magel", "--lang=zh" };
    assert(configure(args, error));
    assert(language() == Language::Zh);
    args = { "magel", "file.mgl", "--lang", "ja" };
    std::vector<char *> values;
    for (auto &arg : args) values.push_back(arg.data());
    const auto normalized = magel_cli::normalize(static_cast<int>(values.size()), values.data());
    assert((normalized == std::vector<std::string>{ "magel", "run", "file.mgl", "--", "--lang", "ja" }));
    std::cout << "Magel i18n unit tests passed\n";
}
