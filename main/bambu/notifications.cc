#include "notifications.h"
#include <algorithm>
namespace bambu {
std::string Notifications::FaultKey(const Snapshot& s){
    std::string key=s.state.value.value_or("")=="FAILED"?"state:FAILED":"";
    if(s.error.value && *s.error.value)key+="/"+std::to_string(*s.error.value);
    if(s.hms.value)for(const auto& f:*s.hms.value)if((f.code>>16)>=1 && (f.code>>16)<=3)key+="/"+f.Key();
    return key;
}
const char* Notifications::Asset(NoticeKind kind){
    return kind==NoticeKind::Started?"notify_start.bbv":kind==NoticeKind::Completed?"notify_finish.bbv":"notify_error.bbv";
}
const char* Notifications::Name(NoticeKind kind){return kind==NoticeKind::Started?"started":kind==NoticeKind::Completed?"completed":"error";}
void Notifications::Observe(const Snapshot& s,int64_t now){
    if(!s.connected || s.demo || !s.state.value){baseline_=false;running_=false;queue_.clear();return;}
    const auto status=*s.state.value,old=previous_.state.value.value_or("");
    const auto job=s.job.value.value_or("");
    const bool same_job=s.job.value==previous_.job.value;
    if(baseline_ && now>=previous_ms_ && now-previous_ms_<=30000){
        if(!same_job){queue_.clear();running_=false;}
        auto add=[&](NoticeKind kind){
            queue_.erase(std::remove_if(queue_.begin(),queue_.end(),[&](const Notice& n){return n.kind==kind;}),queue_.end());
            if(queue_.size()>=4)queue_.pop_front();
            Notice n{kind,job,FaultKey(s),now};
            if(kind==NoticeKind::Error)queue_.push_front(n);else queue_.push_back(n);
        };
        if(status=="RUNNING" && old!="RUNNING" && old!="PAUSE" &&
           (old=="PREPARE" || old=="IDLE" || old=="FINISH" || (!same_job && s.progress.value && *s.progress.value<=1)))add(NoticeKind::Started);
        if(same_job && status=="FINISH" && old!="FINISH" && running_)add(NoticeKind::Completed);
        const auto faults=FaultKey(s),previous_faults=FaultKey(previous_);
        bool new_fault=(s.error.value && *s.error.value && s.error.value!=previous_.error.value) || (status=="FAILED" && old!="FAILED");
        if(s.hms.value)for(const auto& f:*s.hms.value){
            if((f.code>>16)>=1 && (f.code>>16)<=3 &&
               (!previous_.hms.value || std::find(previous_.hms.value->begin(),previous_.hms.value->end(),f)==previous_.hms.value->end()))new_fault=true;
        }
        if(new_fault && !faults.empty() && faults!=previous_faults)add(NoticeKind::Error);
    }
    if(status=="RUNNING")running_=true;else if(status!="PAUSE")running_=false;
    previous_=s;previous_ms_=now;baseline_=true;
}
const Notice* Notifications::Next(const Snapshot& s,int64_t now){
    while(!queue_.empty()){
        const auto& n=queue_.front();const auto status=s.state.value.value_or("");
        const bool valid=s.connected && !s.demo && n.job==s.job.value.value_or("") && now>=n.created_ms && now-n.created_ms<=120000 &&
            (n.kind==NoticeKind::Error?(!n.faults.empty() && n.faults==FaultKey(s)):
             n.kind==NoticeKind::Started?status=="RUNNING":status=="FINISH");
        if(valid)break;
        queue_.pop_front();
    }
    return !queue_.empty() && now-last_sent_>=30000?&queue_.front():nullptr;
}
void Notifications::Accepted(int64_t now){if(!queue_.empty())queue_.pop_front();last_sent_=now;}
}
