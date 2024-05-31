#include "ui/widget.hpp"
#include <cstdarg>
#include <imgui.h>
#include <string>

namespace Humongous
{
void UiWidget::AddBullet(const char* fmt, ...)
{
    std::string outMessage;
    outMessage.resize(320000);
    __builtin_va_list argPtr;
    va_start(argPtr, fmt);
    vsnprintf(&outMessage[0], outMessage.size(), fmt, argPtr);
    va_end(argPtr);

    m_queue.PushText([outMessage]() { ImGui::BulletText(outMessage.c_str()); });
}

void UiWidget::AddText(const char* fmt, ...)
{
    std::string outMessage;
    outMessage.resize(320000);
    __builtin_va_list argPtr;
    va_start(argPtr, fmt);
    vsnprintf(&outMessage[0], outMessage.size(), fmt, argPtr);
    va_end(argPtr);

    m_queue.PushText([outMessage]() { ImGui::Text(outMessage.c_str()); });
}

void UiWidget::Draw()
{
    ImGui::SetNextWindowPos({m_position.x == FLT_MIN ? 0 : m_position.x, m_position.y == FLT_MAX ? 0 : m_position.y});
    ImGui::SetNextWindowSize({m_scale.x == FLT_MIN ? 0 : m_scale.x, m_scale.y == FLT_MAX ? 0 : m_scale.y});

    ImGui::Begin(m_name, &m_show, m_flags);
    m_queue.Flush();

    ImGui::End();
}

} // namespace Humongous
