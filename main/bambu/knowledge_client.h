#pragma once
#include <string>
namespace bambu {
// MCP callers run on the application task; bounded HTTP responses/timeouts.
std::string QueryKnowledge(const std::string& request);
}
