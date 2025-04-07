#include "ui/widget.hpp"
#include "cstdarg"
#include "imgui.h"
#include "string"

namespace Humongous
{

void UiWidget::Draw()
{
    if(m_firstDraw)
    {
        if(m_position.x != FLT_MIN && m_position.y != FLT_MAX) { ImGui::SetNextWindowPos({m_position.x, m_position.y}); }
        if(m_scale.x != FLT_MIN && m_scale.y != FLT_MAX) { ImGui::SetNextWindowSize({m_scale.x, m_scale.y}); }
    }

    ImGui::Begin(m_name, &m_show, m_flags);

    m_queue.Flush();

    ImGui::End();

    m_firstDraw = false;
}

} // namespace Humongous
