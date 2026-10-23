/**
 * @file native_value_text.cpp
 * @brief 运行时值的递归文本表示。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 对默认复合表示中的字符串加引号并转义控制字符。
 * @param text 原始字节文本，可含零字节。
 * @param quote 使用的引号字符。
 * @retval 可区分元素边界的转义文本。
 */
Memory::String valueTextQuote(const Memory::String &text, char quote = '"')
{
    Memory::String out(1, quote);
    const char *hex = "0123456789abcdef";
    for (const unsigned char ch : text)
    {
        if (ch == static_cast<unsigned char>(quote) || ch == '\\')
        {
            out += '\\';
            out += static_cast<char>(ch);
        }
        else if (ch == '\n')
            out += "\\n";
        else if (ch == '\r')
            out += "\\r";
        else if (ch == '\t')
            out += "\\t";
        else if (ch < 32 || ch == 127)
        {
            out += "\\x";
            out += hex[ch >> 4];
            out += hex[ch & 15];
        }
        else
            out += static_cast<char>(ch);
    }
    return out + quote;
}

/**
 * @brief 判断布局是否可能通过自定义方法或 Any 动态载荷调用用户代码。
 * @param shape 当前静态布局。
 * @retval 需要保护外层容器的遍历快照时返回 true。
 */
bool valueTextMayInvoke(const value_text::Shape &shape)
{
    if (shape.formatter || shape.kind == 'O') return true;
    return std::any_of(shape.children.begin(), shape.children.end(), valueTextMayInvoke);
}

/**
 * @brief 默认值文本渲染器，限制递归且只读访问存活存储。
 * @note 默认路径只读；用户覆盖通过标准同步回调执行，容器使用快照隔离重入。
 */
struct ValueTextRenderer
{
    NativeContext &context;
    Memory::Set<std::int64_t> active;
    const Memory::Vector<std::int64_t> *callbacks = nullptr;
    bool failed = false;

    /**
     * @brief 绑定当前 VM 的共同原生上下文。
     * @param ctx 用于读取存储及报告错误的上下文。
     */
    explicit ValueTextRenderer(NativeContext &ctx) : context(ctx) {}

    /**
     * @brief 从合法存储读取一个标量，按布局处理位宽及符号。
     * @param shape 标量布局。
     * @param address 被读取的存储地址。
     * @param value 接收运行时值。
     * @retval 存储有效且位宽可用时返回 true。
     */
    bool load(const value_text::Shape &shape, std::size_t address, Slot &value)
    {
        if (shape.width != 1 && shape.width != 2 && shape.width != 4 && shape.width != 8) return false;
        std::uint64_t bits = 0;
        if (!nativeRead(context, address, reinterpret_cast<char *>(&bits), shape.width))
        {
            context.error = "toString 读取的值存储无效";
            return false;
        }
        if (shape.kind == 'f')
        {
            double number = 0;
            if (shape.width == 4)
            {
                float small = 0;
                std::memcpy(&small, &bits, 4);
                number = small;
            }
            else
                std::memcpy(&number, &bits, 8);
            value = Slot::fromDouble(number);
        }
        else
        {
            if (shape.kind == 'i' && shape.width < 8 && (bits & (std::uint64_t(1) << (shape.width * 8 - 1))))
                bits |= (~std::uint64_t(0)) << (shape.width * 8);
            value = Slot::fromInt(static_cast<std::int64_t>(bits));
        }
        return true;
    }

    /**
     * @brief 将原始聚合或容器保存的展平字段中的一个字段转换为文本。
     * @param shape 字段布局及相对偏移。
     * @param address 原始聚合地址。
     * @param fields 容器中的展平字段；为空时读取原始地址。
     * @param base 展平字段起始索引。
     * @param depth 当前递归深度。
     * @retval 字段的带边界文本。
     */
    Memory::String field(const value_text::Shape &shape, std::size_t address,
                         const Memory::Vector<ObjectValue> *fields, std::size_t base, std::size_t depth)
    {
        const bool aggregate = shape.kind == 'S' || shape.kind == 'T' || shape.kind == 'N' || shape.kind == 'A';
        if (fields)
        {
            const auto index = base + shape.offset / 8;
            if (aggregate) return render(shape, Slot::fromInt(0), nullptr, depth, true, fields, index);
            if (index >= fields->size())
            {
                context.error = "toString 的容器字段布局越界";
                return {};
            }
            return render(shape, Slot::fromInt(0), &(*fields)[index], depth, true);
        }
        if (shape.offset > std::numeric_limits<std::size_t>::max() - address)
        {
            context.error = "toString 的字段地址溢出";
            return {};
        }
        const auto position = address + shape.offset;
        if (aggregate) return render(shape, Slot::fromInt(static_cast<std::int64_t>(position)), nullptr, depth, true);
        Slot value;
        if (!load(shape, position, value)) return {};
        return render(shape, value, nullptr, depth, true);
    }

    /**
     * @brief 调用嵌套值的只读 toString，展平容器字段使用独立的临时视图。
     * @param shape 含回调索引的类型布局。
     * @param value 原始聚合地址。
     * @param fields 容器保存的字段快照。
     * @param base 快照中的起始字段。
     * @retval 用户返回的文本；失败保留原始回调诊断。
     */
    Memory::String custom(const value_text::Shape &shape, Slot value, const Memory::Vector<ObjectValue> *fields, std::size_t base)
    {
        if (shape.formatter == std::numeric_limits<std::size_t>::max())
        {
            context.error = "嵌套值的 toString 必须只读，不能修改接收者";
            return {};
        }
        if (!callbacks || shape.formatter == 0 || shape.formatter > callbacks->size() / 2 || !context.invoker)
        {
            context.error = "toString 的用户回调未绑定";
            return {};
        }
        const auto index = (shape.formatter - 1) * 2;
        RuntimeObject::ElementView view;
        struct ViewGuard
        {
            NativeContext &context;
            RuntimeObject::ElementView &view;
            /**
             * @brief 清理临时视图的拥有叶子并归还字节存储。
             */
            ~ViewGuard()
            {
                if (view.address)
                {
                    nativeReleaseViewContents(context, view);
                    context.heap->release(view.address);
                }
            }
        } guard{ context, view };
        if (fields)
        {
            const auto count = shape.width / 8;
            if (base > fields->size() || count > fields->size() - base || (*callbacks)[index + 1] <= 1)
            {
                context.error = "toString 的聚合回调存储布局无效";
                return {};
            }
            view.address = nativeAllocate(context, std::max<std::size_t>(8, shape.width));
            view.objectFlag = (*callbacks)[index + 1];
            if (!view.address) return {};
            Memory::Vector<char> zero(std::max<std::size_t>(8, shape.width), 0);
            if (!nativeWrite(context, view.address, zero.data(), zero.size())) return {};
            ObjectValue projection;
            projection.isStruct = true;
            projection.fields.assign(fields->begin() + base, fields->begin() + base + count);
            if (!nativeWriteView(context, view, projection)) return {};
            value = Slot::fromInt(static_cast<std::int64_t>(view.address));
        }
        NativeValue produced;
        if (!(*context.invoker)((*callbacks)[index], { NativeValue::fromInt(nativeInt(value)) }, produced))
        {
            failed = true;
            return {};
        }
        Memory::String text;
        const auto address = static_cast<std::size_t>(produced.asInt());
        const bool valid = nativeReadCString(context, address, &text);
        if (context.heap) context.heap->release(address);
        if (!valid) context.error = "toString 回调没有返回有效字符串";
        return text;
    }

    /**
     * @brief 按类型布局渲染值，保留结构体、元组及各类容器的边界。
     * @param shape 静态类型布局。
     * @param value 源标量、句柄或聚合地址。
     * @param stored 容器或 Any 内保存的只读值。
     * @param depth 当前递归深度。
     * @param nested 是否对字符串和字符添加元素引号。
     * @param fields 嵌入式聚合的展平字段。
     * @param base 展平字段起始索引。
     * @retval 新构造的文本；错误写入上下文。
     */
    Memory::String render(const value_text::Shape &shape, Slot value, const ObjectValue *stored = nullptr,
                          std::size_t depth = 0, bool nested = false,
                          const Memory::Vector<ObjectValue> *fields = nullptr, std::size_t base = 0)
    {
        if (depth > 64) return "...";
        if (stored)
        {
            value = stored->isFloat ? Slot::fromDouble(stored->real) : Slot::fromInt(stored->integer);
            if (stored->isStruct)
            {
                fields = &stored->fields;
                base = 0;
            }
        }
        const auto integer = nativeInt(value);
        if (shape.formatter) return custom(shape, value, fields, base);
        if (shape.kind == 's')
        {
            Memory::String text;
            if (stored && stored->isText)
                text = stored->text;
            else if (!nativeReadCString(context, static_cast<std::size_t>(integer), &text))
            {
                context.error = "toString 读不到字符串";
                return {};
            }
            return nested ? valueTextQuote(text) : text;
        }
        if (shape.kind == 'c')
        {
            const Memory::String text(1, static_cast<char>(integer));
            return nested ? valueTextQuote(text, '\'') : text;
        }
        if (shape.kind == 'b') return integer ? "true" : "false";
        if (shape.kind == 'i') return Memory::toString(integer);
        if (shape.kind == 'u') return Memory::toString(static_cast<std::uint64_t>(integer));
        if (shape.kind == 'f')
        {
            char buffer[128];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value.isFloat ? value.real : static_cast<double>(value.integer));
            return result.ec == std::errc{} ? Memory::String(buffer, result.ptr) : Memory::String("nan");
        }
        if (shape.kind == 'S' || shape.kind == 'T' || shape.kind == 'N')
        {
            const value_text::Shape *selected = &shape;
            Memory::String prefix = shape.kind == 'T' ? "(" : "{";
            const char suffix = shape.kind == 'T' ? ')' : '}';
            if (shape.kind == 'N')
            {
                value_text::Shape tag;
                tag.kind = 'i';
                Slot discriminant;
                if (fields)
                {
                    if (base >= fields->size())
                    {
                        context.error = "枚举文本表示的标签越界";
                        return {};
                    }
                    discriminant = Slot::fromInt((*fields)[base].integer);
                }
                else if (!load(tag, static_cast<std::size_t>(integer), discriminant))
                    return {};
                selected = nullptr;
                for (const auto &variant : shape.children)
                    if (variant.count == static_cast<std::size_t>(nativeInt(discriminant)))
                    {
                        selected = &variant;
                        break;
                    }
                if (!selected)
                {
                    context.error = "枚举文本表示的标签无效";
                    return {};
                }
                prefix = selected->name + "(";
            }
            Memory::String text = prefix;
            for (std::size_t index = 0; index < selected->children.size(); ++index)
            {
                if (index) text += ", ";
                text += field(selected->children[index], static_cast<std::size_t>(integer), fields, base, depth + 1);
                if (!context.error.empty() || failed) return {};
            }
            if (shape.kind == 'T' && selected->children.size() == 1) text += ',';
            return text + (shape.kind == 'N' ? ')' : suffix);
        }
        if (shape.kind == 'A' || shape.kind == 'a')
        {
            if (shape.children.size() != 1 || fields)
            {
                context.error = "数组文本表示缺少有效布局";
                return {};
            }
            std::size_t count = shape.count;
            if (shape.kind == 'a')
            {
                value_text::Shape length;
                length.kind = 'i';
                Slot read;
                if (integer < 8 || !load(length, static_cast<std::size_t>(integer - 8), read) || nativeInt(read) < 0) return {};
                count = static_cast<std::size_t>(nativeInt(read));
            }
            Memory::String text = "[";
            for (std::size_t index = 0; index < count; ++index)
            {
                if (shape.width && index > (std::numeric_limits<std::size_t>::max() - static_cast<std::size_t>(integer)) / shape.width)
                {
                    context.error = "数组文本表示的地址溢出";
                    return {};
                }
                if (index) text += ", ";
                auto child = shape.children[0];
                child.offset = index * shape.width;
                text += field(child, static_cast<std::size_t>(integer), nullptr, 0, depth + 1);
                if (!context.error.empty() || failed) return {};
            }
            return text + ']';
        }
        if (shape.kind == 'R' || shape.kind == 'F' || shape.kind == 'H' || shape.kind == 'X')
            return "<" + shape.name + ">";
        RuntimeObject *object = context.objects->find(integer);
        if (!object)
        {
            context.error = "toString 访问了无效对象句柄";
            return {};
        }
        if (!active.insert(integer).second) return "<cycle>";
        struct ActiveGuard
        {
            Memory::Set<std::int64_t> &active;
            std::int64_t handle;
            /**
             * @brief 离开当前渲染分支时撤销循环检测标记。
             */
            ~ActiveGuard() { active.erase(handle); }
        } guard{ active, integer };
        struct SnapshotGuard
        {
            NativeContext &context;
            std::int64_t handle = 0;
            /**
             * @brief 在全部回调结束后释放重入隔离快照。
             */
            ~SnapshotGuard()
            {
                if (handle) context.objects->release(handle);
            }
        } snapshot{ context };
        if ((valueTextMayInvoke(shape) || (callbacks && !callbacks->empty()) || !object->objTextCallbacks.empty()) && ((object->kind == ObjectKind::NativeRecord && object->nativeType && object->nativeType->textView) || object->kind == ObjectKind::Obj))
        {
            Memory::String error;
            snapshot.handle = context.objects->copyHandle(integer, &error);
            if (!snapshot.handle)
            {
                context.error = error;
                return {};
            }
            object = context.objects->find(snapshot.handle);
        }
        if (!nativeRefreshViews(context, *object)) return {};
        if (object->kind == ObjectKind::NativeRecord && object->nativeType && object->nativeType->toString)
            return object->nativeType->toString(object->nativeRecord.get());
        if (object->kind == ObjectKind::Obj)
        {
            if (object->objTag == 0) return "Any()";
            value_text::Shape child;
            if (!object->objTextShape.empty())
            {
                std::size_t at = 0, budget = 4096;
                if (!value_text::decode(object->objTextShape, at, child, budget) || at != object->objTextShape.size())
                {
                    context.error = "Any 文本表示的布局无效";
                    return {};
                }
            }
            else
            {
                child.name = object->objTypeName;
                child.kind = object->objValue.isText ? 's' : object->objValue.isFloat                          ? 'f' :
                                                         object->objOwnsObject                                 ? 'O' :
                                                         object->objTypeName == "bool"                         ? 'b' :
                                                         object->objTypeName == "char"                         ? 'c' :
                                                         object->objTypeName.find('&') != Memory::String::npos ? 'R' :
                                                                                                                 'i';
            }
            const auto saved = callbacks;
            callbacks = &object->objTextCallbacks;
            const auto text = render(child, Slot::fromInt(0), &object->objValue, depth + 1, nested);
            callbacks = saved;
            return text;
        }
        if (object->kind == ObjectKind::NativeRecord && object->nativeType && object->nativeType->textView)
        {
            NativeCallContextImpl host(context);
            if (object->nativeType->prepare && !object->nativeType->prepare(host, object->nativeRecord.get())) return {};
            NativeRecordTextView view;
            if (!object->nativeType->textView(host, object->nativeRecord.get(), view)) return {};
            Memory::String text = view.prefix;
            for (std::size_t index = 0; index < view.rows.size(); ++index)
            {
                if (index) text += view.separator;
                const auto &row = view.rows[index];
                if (row.size() != shape.children.size())
                {
                    context.error = "宿主记录文本字段与静态布局不一致";
                    return {};
                }
                for (std::size_t field = 0; field < row.size(); ++field)
                {
                    if (field) text += view.fieldSeparator;
                    text += render(shape.children[field], Slot::fromInt(0), &row[field], depth + 1, true);
                    if (!context.error.empty() || failed) return {};
                }
            }
            return text + view.suffix;
        }
        return "<" + shape.name + ">";
    }
};

/**
 * @brief 将任意受支持值的默认文本表示分配为拥有型字符串。
 * @param ctx VM 共用的原生上下文。
 * @param args 源值及静态布局字符串地址。
 * @retval 新字符串地址；失败时写入诊断并返回零。
 */
Slot valueToString(NativeContext &ctx, const Memory::Vector<Slot> &args)
{
    if (args.size() < 2u || args.size() % 2u)
    {
        ctx.error = "默认文本转换的回调参数不完整";
        return Slot::fromInt(0);
    }
    Memory::String descriptor;
    value_text::Shape shape;
    std::size_t at = 0, budget = 4096;
    if (!nativeReadCString(ctx, static_cast<std::size_t>(nativeInt(args[1])), &descriptor) || !value_text::decode(descriptor, at, shape, budget) || at != descriptor.size())
    {
        ctx.error = "默认文本表示的布局无效";
        return Slot::fromInt(0);
    }
    Memory::Vector<std::int64_t> callbacks;
    for (std::size_t index = 2; index < args.size(); ++index) callbacks.push_back(nativeInt(args[index]));
    ValueTextRenderer renderer(ctx);
    renderer.callbacks = &callbacks;
    const auto text = renderer.render(shape, args[0]);
    if (!ctx.error.empty() || renderer.failed) return Slot::fromInt(0);
    return Slot::fromInt(static_cast<std::int64_t>(nativeAllocString(ctx, text)));
}
}  // namespace runtimeDetail
}  // namespace stackvm
