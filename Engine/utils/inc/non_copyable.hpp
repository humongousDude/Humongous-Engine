#pragma once

namespace Humongous
{
class NonCopyable
{
protected:
    NonCopyable() = default;
    virtual ~NonCopyable() = default;

public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&&) = default;
};
} // namespace Humongous
