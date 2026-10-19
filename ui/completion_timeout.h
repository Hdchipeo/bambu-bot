#pragma once
#include <cstdint>
#include <string>

namespace bambu::ui {
// One celebration per observed FINISH/job. Called under the display lock.
class CompletionTimeout {
public:
    static constexpr uint32_t kDurationMs=30000;
    bool Show(bool connected,const std::string& state,const std::string& job,uint32_t now){
        // Sparse/stale telemetry and reconnects must not restart a celebration.
        if(!job.empty()){
            if(!job_.empty() && job!=job_)finished_=false;
            job_=job;
        }
        if(!connected || state.empty())return false;
        if(state!="FINISH"){
            if(state=="RUNNING" || state=="PREPARE")finished_=false;
            return false;
        }
        if(!finished_){finished_=true;started_=now;}
        return uint32_t(now-started_)<kDurationMs;
    }
private:
    std::string job_;
    bool finished_=false;
    uint32_t started_=0;
};
}
