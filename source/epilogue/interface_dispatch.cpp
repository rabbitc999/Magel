/* ---------- 接口引用上的方法调用（运行期按类型号分派） ---------- */

/**
 * @brief 在接口声明的函数表里找一个方法。
 * @param iface 接口名。
 * @param methodName 方法名。
 * @param result 输出：接口声明的返回类型。
 * @param params 输出：接口声明的形参类型表。
 * @return 找到返回 true。
 */
bool FindInterfaceMethod(const Memory::String &iface, const Memory::String &methodName,
                         std::shared_ptr<MType> *result,
                         Memory::Vector<std::shared_ptr<MType>> *params)
{
    const auto found = g_interfaces.find(iface);
    if (found == g_interfaces.end()) return false;
    for (const MInterfaceMethod &method : found->second)
    {
        if (method.name != methodName || method.isStatic) continue;
        *result = method.result;
        *params = method.params;
        return true;
    }
    return false;
}
