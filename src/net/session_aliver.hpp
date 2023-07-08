#ifndef SESSION_ALIVER_HPP
#define SESSION_ALIVER_HPP
#include <stdint.h>
#include <stddef.h>

namespace cpp_streamer
{
class SessionAliver
{
public:
    SessionAliver(int try_max = 4):try_max_(try_max) {
    }
    ~SessionAliver() {

    }

public:
    void KeepAlive() {
        not_alive_cnt_ = 0;
    }

    bool IsAlive() {
        if (not_alive_cnt_++ > try_max_) {
            return false;
        }
        return true;
    }

    void UpdateMax(int max) { try_max_ = max; }

private:
    int not_alive_cnt_ = 0;
    int try_max_ = 0;
};
}
#endif
