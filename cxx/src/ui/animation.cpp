#include "ui/animation.h"

#include <algorithm>

namespace swiftlist::ui {

Animation::Animation() = default;

Animation::~Animation() {
    Stop();
}

void Animation::Start(float from, float to, uint32_t durationMs, Easing easing,
                       UpdateFn update, CompleteFn complete) {
    from_ = from;
    to_ = to;
    current_ = from;
    durationMs_ = durationMs;
    elapsedMs_ = 0;
    easing_ = easing;
    update_ = std::move(update);
    complete_ = std::move(complete);
    running_ = true;
}

void Animation::Stop() {
    running_ = false;
}

void Animation::Tick() {
    if (!running_) return;

    elapsedMs_ += 16;
    float t = durationMs_ > 0
                  ? std::clamp(static_cast<float>(elapsedMs_) / durationMs_, 0.0f, 1.0f)
                  : 1.0f;

    float easedT = EaseValue(t, easing_);
    current_ = from_ + (to_ - from_) * easedT;

    if (update_) update_(current_);

    if (t >= 1.0f) {
        running_ = false;
        if (complete_) complete_();
    }
}

} // namespace swiftlist::ui
