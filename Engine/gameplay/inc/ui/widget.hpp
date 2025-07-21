#pragma once

#include "defines.hpp"
#include <Eigen/Dense>
#include <deque>
#include <functional>
#include <imgui.h>

namespace Humongous
{
class UiWidget
{
    struct TextQueue
    {
        std::deque<std::function<void()>> texts;

        void PushText(const std::function<void()>& text) { texts.push_back(text); }

        void Flush() const
        {
            for(auto& text: texts) { text(); }
        }

        void Clear() { texts.clear(); }
    };

public:
    UiWidget(const char* name, const bool show, const Eigen::Vector2f position, const Eigen::Vector2f scale, const ImGuiWindowFlags flags)
        : m_position{position}, m_scale{scale}, m_name{name}, m_show{show}, m_flags{flags}
    {
    }

    UiWidget() : m_position{0, 0}, m_scale{10, 10}, m_name{"You should probably name this :)"}, m_show{true}, m_flags{0} {}

    // There HAS to be a better way to do this than just a bunch of lambda functions
    void Add(const std::function<void()>& func) { m_queue.PushText(func); };

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

    Eigen::Vector2f  m_position{FLT_MIN, FLT_MAX};
    Eigen::Vector2f  m_scale{FLT_MIN, FLT_MAX};
    const char*      m_name{nullptr};
    bool             m_show{true};
    ImGuiWindowFlags m_flags{};

    b32 m_firstDraw{true};
};
} // namespace Humongous
