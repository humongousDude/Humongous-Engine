#pragma once

#include <deque>
#include <functional>
#include <glm/ext/vector_float2.hpp>
#include <imgui.h>

namespace Humongous
{
class UiWidget
{
    struct TextQueue
    {
        std::deque<std::function<void()>> texts;

        void PushText(std::function<void()> text) { texts.push_front(text); }

        void Flush()
        {
            for(auto& text: texts) { text(); }
            // texts.clear();
        }

        void Clear() { texts.clear(); }
    };

public:
    UiWidget(const char* name, bool show, glm::vec2 position, glm::vec2 scale, ImGuiWindowFlags flags)
        : m_name{name}, m_show{show}, m_position{position}, m_scale{scale}, m_flags{flags}
    {
    }

    UiWidget() : m_name{"You should probably name this :)"}, m_show{true}, m_position{0, 0}, m_scale{10, 10}, m_flags{0} {}

    void AddBullet(const char* fmt, ...);
    void AddText(const char* fmt, ...);

    void Draw();

    glm::vec2        m_position{FLT_MIN, FLT_MAX};
    glm::vec2        m_scale{FLT_MIN, FLT_MAX};
    const char*      m_name{nullptr};
    bool             m_show{true};
    ImGuiWindowFlags m_flags;

private:
    TextQueue m_queue;
};
} // namespace Humongous
