#include "hms.h"
#include <cstdio>
namespace bambu {
std::string HmsFault::Key() const {
    char text[17];snprintf(text,sizeof(text),"%08lX%08lX",static_cast<unsigned long>(attr),static_cast<unsigned long>(code));
    return text;
}
std::string HmsFault::DisplayCode() const {
    const auto key=Key();return key.substr(0,4)+"-"+key.substr(4,4)+"-"+key.substr(8,4)+"-"+key.substr(12,4);
}
const char* HmsFault::Severity() const {
    switch(code>>16){case 1:return "fatal";case 2:return "serious";case 3:return "common";case 4:return "info";default:return "unknown";}
}
}
