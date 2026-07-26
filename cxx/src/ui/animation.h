#pragma once

#include <chrono>
#include <cmath>
#include <functional>

namespace swiftlist::ui {

enum class Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    CubicOut,
    CubicInOut,
};

inline float EaseValue(float t, Easing easing) {
    switch (easing) {
    case Easing::Linear: return t;
    case Easing::EaseIn: return t * t;
    case Easing::EaseOut: return 1.0f - (1.0f - t) * (1.0f - t);
    case Easing::EaseInOut:
        return t < 0.5f ? 2.0f * t * t
                        : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    case Easing::CubicOut: return 1.0f - std::pow(1.0f - t, 3.0f);
    case Easing::CubicInOut:
        return t < 0.5f ? 4.0f * t * t * t
                        : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }
    return t;
}

class Animation {
public:
    using UpdateFn = std::function<void(float value)>;
    using CompleteFn = std::function<void()>;

    Animation();
    ~Animation();

    void Start(float from, float to, uint32_t durationMs, Easing easing,
               UpdateFn update, CompleteFn complete = nullptr);
    void Stop();
    bool IsRunning() const { return running_; }

    void Tick();

private:
    float from_ = 0;
    float to_ = 0;
    float current_ = 0;
    uint32_t durationMs_ = 0;
    uint32_t elapsedMs_ = 0;
    Easing easing_ = Easing::Linear;
    UpdateFn update_;
    CompleteFn complete_;
    bool running_ = false;
};

} // namespace swiftlist::ui
