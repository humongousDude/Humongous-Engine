#pragma once

#include "defines.hpp"
#include <algorithm>
#include <functional>
#include <vector>

namespace Humongous
{

template <typename... Args> class Event
{
private:
    using Callback = std::function<void(Args...)>;
    using SubscriptionID = u64;

    struct Subscription
    {
        SubscriptionID id;
        Callback       callback;
    };

public:
    SubscriptionID Subscribe(Callback callback)
    {
        SubscriptionID id = m_nextId++;
        m_subscriptions.push_back({id, callback});
        return id;
    }

    void Unsubscribe(SubscriptionID id)
    {
        m_subscriptions.erase(
            std::remove_if(m_subscriptions.begin(), m_subscriptions.end(), [id](const Subscription& sub) { return sub.id == id; }),
            m_subscriptions.end());
    }

    void Invoke(Args... args) const
    {
        for(const auto& sub: m_subscriptions)
        {
            if(sub.callback) { sub.callback(args...); }
        }
    }

private:
    std::vector<Subscription> m_subscriptions;
    SubscriptionID            m_nextId = 0;
};
} // namespace Humongous
