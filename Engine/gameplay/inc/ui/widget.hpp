#pragma once

#include "defines.hpp"
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

        void PushText(std::function<void()> text) { texts.push_back(text); }

        void Flush()
        {
            for(auto& text: texts) { text(); }
        }

        void Clear() { texts.clear(); }
    };

public:
    UiWidget(const char* name, bool show, glm::vec2 position, glm::vec2 scale, ImGuiWindowFlags flags)
        : m_name{name}, m_show{show}, m_position{position}, m_scale{scale}, m_flags{flags}
    {
    }

    UiWidget() : m_name{"You should probably name this :)"}, m_show{true}, m_position{0, 0}, m_scale{10, 10}, m_flags{0} {}

    // There HAS to be a better way to do this than just a bunch of lambda functions
    void Add(std::function<void()> func) { m_queue.PushText(func); };

    /***
     *  Iterate through the queue and call the functions stored. This does *NOT* clear the queue
     */
    void Draw();

    /***
     *  Clear the queue. This does *NOT* draw anything
     */
    void ClearQueue() { m_queue.Clear(); };

private:
    TextQueue m_queue;

    glm::vec2        m_position{FLT_MIN, FLT_MAX};
    glm::vec2        m_scale{FLT_MIN, FLT_MAX};
    const char*      m_name{nullptr};
    bool             m_show{true};
    ImGuiWindowFlags m_flags{};

    b32 m_firstDraw{true};
};
} // namespace Humongous
