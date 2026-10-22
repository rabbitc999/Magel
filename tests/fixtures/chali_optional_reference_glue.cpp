#include "chali_glue.hpp"

/**
 * @brief 注册可选引用的跨边界示例。
 * @param registry 胶水表。
 * @param error 注册错误。
 * @return 注册成功。
 */
bool bindChaLiGlue(chali_glue::Registry &registry, Memory::String *error)
{
    (void)error;
    using Reference = chali_glue::Reference<std::int64_t>;
    using OptionalReference = std::optional<Reference>;
    registry.bindTyped<OptionalReference, bool, Reference>("demo_reference_optional",
                                                           [](bool present, Reference value) -> OptionalReference {
                                                               return present ? OptionalReference(value) : std::nullopt;
                                                           });
    registry.bindTypedWithContext<std::int64_t, OptionalReference>("demo_reference_read",
                                                                   [](chali_glue::Context &context, const OptionalReference &value) {
                                                                       return value ? value->read(context) : std::int64_t(-1);
                                                                   });
    registry.bindTypedWithContext<void, OptionalReference>("demo_reference_write",
                                                           [](chali_glue::Context &context, const OptionalReference &value) {
                                                               if (value) value->write(context, value->read(context) + 1);
                                                           });
    return true;
}
