#pragma once

#include <mutex>
#include <memory>

namespace hft {

template<typename T>
class Singleton {
public:
    static T& getInstance() {
        std::call_once(once_flag_, []() {
            instance_ = std::unique_ptr<T>(new T());
        });
        return *instance_;
    }

    // Delete copy constructor and assignment operator
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    virtual ~Singleton() = default;
protected:
    Singleton() = default;

private:
    static std::once_flag once_flag_;
    static std::unique_ptr<T> instance_;
};

template<typename T>
std::once_flag Singleton<T>::once_flag_;

template<typename T>
std::unique_ptr<T> Singleton<T>::instance_;

} // namespace hft 