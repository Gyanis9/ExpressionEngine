#include <ExpressionEngine/Base/Exception.h>

#include <format>
#include <utility>

namespace ExpressionEngine::Base
{

Exception::Exception(std::string message, const std::source_location& location)
    // what() 与 message() 同源：只保存一份文案，避免两处描述各自漂移
    : std::runtime_error(message)
    , m_message(std::move(message))
    , m_file(location.file_name())
    , m_sourceLine(static_cast<int>(location.line()))
    , m_function(location.function_name())
{}

std::string Exception::toString() const
{
    // 组合格式只在此处实现一次，运行期故障这一支的所有派生类型共用
    return std::format("{}（{}:{} in {}）", m_message, m_file, m_sourceLine, m_function);
}

}  // namespace ExpressionEngine::Base
