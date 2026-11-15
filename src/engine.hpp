#pragma once
#include "model.hpp"
#include <functional>
#include <memory>
namespace mpg {
class Monitor {
public:
    explicit Monitor(std::function<void()> changed);
    ~Monitor();
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;
    void start(const Settings& settings);
    void update(const Settings& settings);
    Snapshot snapshot() const;
    void stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
