#include "notifications.h"
#include "voice_prompt.h"
#include "../ui/completion_timeout.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace bambu;
Snapshot Status(const char* state,int64_t now,const char* job="one"){
    Snapshot s;s.connected=true;s.demo=false;s.state={std::string(state),now};s.job={std::string(job),now};
    s.error={0,now};s.hms={std::vector<HmsFault>{},now};return s;
}
int main(){
    bambu::ui::CompletionTimeout completion;
    assert(!completion.Show(true,"RUNNING","one",0));
    assert(completion.Show(true,"FINISH","one",100));
    assert(completion.Show(true,"FINISH","one",30099));
    assert(!completion.Show(true,"FINISH","one",30100));
    assert(!completion.Show(true,"FINISH","one",60000)); // Repeated MQTT updates cannot reset timeout.
    assert(!completion.Show(false,"","",61000));
    assert(!completion.Show(true,"FINISH","one",62000)); // Reconnect same job stays expired.
    assert(!completion.Show(true,"IDLE","one",62100));
    assert(!completion.Show(true,"FINISH","one",62200)); // An idle/finish status bounce is not a new print.
    assert(completion.Show(true,"FINISH","two",63000)); // Next job gets its own celebration.
    assert(!completion.Show(true,"PAUSE","two",64000));
    assert(!completion.Show(true,"RUNNING","two",65000));
    assert(completion.Show(true,"FINISH","two",66000)); // IDs can be reused by the printer.
    bambu::ui::CompletionTimeout rollover;
    assert(rollover.Show(true,"FINISH","",0xfffffff0u));
    assert(rollover.Show(true,"FINISH","",0x10u));
    assert(!rollover.Show(true,"FINISH","",0x8000u));
    bambu::ui::CompletionTimeout sparse;
    assert(sparse.Show(true,"FINISH","",0));
    assert(!sparse.Show(true,"FINISH","known-later",30000)); // Late job metadata does not rearm.

    Notifications n;auto s=Status("RUNNING",0);n.Observe(s,0);assert(!n.Next(s,0)); // Mid-print boot.
    s=Status("PAUSE",1000);n.Observe(s,1000);s=Status("RUNNING",2000);n.Observe(s,2000);assert(!n.Next(s,2000));
    s=Status("FINISH",3000);n.Observe(s,3000);assert(n.Next(s,3000)->kind==NoticeKind::Completed);n.Accepted(3000);
    n.Observe(s,4000);assert(!n.Next(s,4000));
    s=Status("PREPARE",40000,"two");n.Observe(s,40000);s=Status("RUNNING",41000,"two");n.Observe(s,41000);
    assert(n.Next(s,41000)->kind==NoticeKind::Started);
    // A new fault supersedes start; removal/clear while waiting must suppress stale advice.
    s.error={123,42000};n.Observe(s,42000);assert(n.Next(s,42000)->kind==NoticeKind::Error);
    s.error={0,43000};n.Observe(s,43000);assert(n.Next(s,43000)->kind==NoticeKind::Started);n.Accepted(43000);
    s.hms={std::vector<HmsFault>{{0x03000100,0x00010007}},44000};n.Observe(s,44000);assert(!n.Next(s,44000)); // Cooldown.
    assert(n.Next(s,74000)->kind==NoticeKind::Error);n.Accepted(74000);
    n.Observe(s,75000);assert(!n.Next(s,75000));
    s.hms={std::vector<HmsFault>{},76000};n.Observe(s,76000);assert(n.Size()==0);
    s.hms={std::vector<HmsFault>{{1,0x00040001}},77000};n.Observe(s,77000);assert(n.Size()==0); // Informational.
    s.hms={std::vector<HmsFault>{{1,0x00020001}},78000};n.Observe(s,78000);assert(n.Size()==1);
    assert(!n.Next(s,200001)); // TTL.
    s.connected=false;n.Observe(s,210000);s.connected=true;n.Observe(s,211000);assert(n.Size()==0); // Reconnect baseline.
    s=Status("PREPARE",212000);n.Observe(s,212000);s=Status("RUNNING",213000);n.Observe(s,213000);
    assert(n.Size()==1);s.demo=true;n.Observe(s,214000);assert(n.Size()==0);
    Notifications missed;auto a=Status("PREPARE",0);missed.Observe(a,0);a=Status("RUNNING",31001);missed.Observe(a,31001);assert(!missed.Next(a,31001));
    Notifications failed;auto failure=Status("RUNNING",0);failed.Observe(failure,0);
    failure=Status("FAILED",1000);failed.Observe(failure,1000);assert(failed.Next(failure,1000)->kind==NoticeKind::Error);
    PrinterState bmcu(true);bmcu.SetDemo(false);bmcu.SetConnection(true);
    bmcu.Apply(R"({"print":{"gcode_state":"RUNNING"}})",0);
    Notifications filtered;filtered.Observe(bmcu.GetFreshSnapshot(0),0);
    bmcu.Apply(R"({"print":{"hms":[{"attr":83887104,"code":65604}]}})",1000);
    auto filtered_snapshot=bmcu.GetFreshSnapshot(1000);filtered.Observe(filtered_snapshot,1000);
    assert(!filtered.Next(filtered_snapshot,1000));
    bmcu.Apply(R"({"print":{"hms":[{"attr":83887104,"code":65604},{"attr":50331904,"code":65543}]}})",2000);
    filtered_snapshot=bmcu.GetFreshSnapshot(2000);filtered.Observe(filtered_snapshot,2000);
    assert(filtered.Next(filtered_snapshot,2000)->kind==NoticeKind::Error);
    assert((HmsFault{0x03000100,0x00010007}.DisplayCode()=="0300-0100-0001-0007"));
    assert(std::string(HmsFault{0,0x00020001}.Severity())=="serious");
    for(const auto* kind:{"start","finish","error"}){
        std::ifstream f(std::string("ui/animation_assets/notify_")+kind+".bbv",std::ios::binary);
        std::vector<uint8_t> data(std::istreambuf_iterator<char>(f),{});VoicePrompt prompt;
        assert(prompt.Load(data.data(),data.size()));const size_t count=prompt.Remaining();assert(count>50);
        const uint8_t* packet;size_t size;size_t decoded=0;
        while(prompt.Next(packet,size)){assert(size>0 && size<=1275);++decoded;}
        assert(decoded==count);assert(!prompt.Next(packet,size));
        assert(!prompt.Load(data.data(),data.size()-1));data[0]=0;assert(!prompt.Load(data.data(),data.size()));
        assert(!prompt.Next(packet,size));
    }
    std::cout<<"PASS: proactive event baseline/dedup/cooldown/expiry, HMS and voice packets\n";
}
