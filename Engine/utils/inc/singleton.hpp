#pragma once

namespace Humongous
{
template <class T> class Singleton
{
public:
    static T& Instance()
    {
        static T instance;
        return instance;
    }

    static T& Get();

protected:
    explicit Singleton<T>() = default;
};

template <typename T> T& Singleton<T>::Get()
{
    static T instance;
    return instance;
}

} // namespace Humongous
