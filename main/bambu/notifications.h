#pragma once
#include "printer_state.h"
#include <deque>
namespace bambu {
enum class NoticeKind { Started, Completed, Error };
struct Notice { NoticeKind kind;std::string job,faults;int64_t created_ms; };
// Application-thread only. Observe fresh snapshots; never announce a boot/reconnect baseline.
class Notifications {
public:
    void Observe(const Snapshot& snapshot,int64_t now);
    const Notice* Next(const Snapshot& snapshot,int64_t now);
    void Accepted(int64_t now);
    size_t Size() const { return queue_.size(); }
    static std::string FaultKey(const Snapshot& snapshot);
    static const char* Asset(NoticeKind kind);
    static const char* Name(NoticeKind kind);
private:
    Snapshot previous_;
    int64_t previous_ms_=-1,last_sent_=-60000;
    bool baseline_=false,running_=false;
    std::deque<Notice> queue_;
};
}
