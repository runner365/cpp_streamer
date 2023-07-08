#ifndef TIMER_HPP
#define TIMER_HPP
#include <uv.h>
#include <stdint.h>

inline void OnUvTimerCallback(uv_timer_t *handle);

class TimerInterface
{
friend void OnUvTimerCallback(uv_timer_t *handle);

public:
    TimerInterface(uv_loop_t* loop, uint32_t timeout_ms):timeout_ms_(timeout_ms)
    {
        uv_timer_init(loop, &timer_);
        timer_.data = this;
    }

    virtual ~TimerInterface() {
        StopTimer();
    }

public:
    virtual void OnTimer() = 0;

public:
    void StartTimer() {
        if(running_) {
            return;
        }
        running_ = true;
        uv_timer_start(&timer_, OnUvTimerCallback, timeout_ms_, timeout_ms_);
    }

    void StopTimer() {
        if (!running_) {
            return;
        }
        running_ = false;
        uv_timer_stop(&timer_);
    }

private:
    uv_timer_t timer_;
    uint32_t timeout_ms_;
    bool running_ = false;
};

inline void OnUvTimerCallback(uv_timer_t *handle) {
    TimerInterface* timer = (TimerInterface*)handle->data;
    if (timer && timer->running_) {
        timer->OnTimer();
    }
}

#endif
