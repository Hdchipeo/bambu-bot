#include "knowledge_client.h"
#include "configuration.h"
#include "board.h"
#include <cJSON.h>
namespace bambu {
std::string QueryKnowledge(const std::string& request){
    if(std::string(BAMBU_WIKI_URL).empty())return R"({"available":false,"reason":"wiki_backend_not_configured","sources":[]})";
    auto http=Board::GetInstance().GetNetwork()->CreateHttp(0);
    if(!http)return R"({"available":false,"reason":"http_unavailable","sources":[]})";
    http->SetTimeout(5000);http->SetHeader("Content-Type","application/json");
    if(std::string(BAMBU_WIKI_TOKEN).size())http->SetHeader("Authorization",std::string("Bearer ")+BAMBU_WIKI_TOKEN);
    http->SetContent(std::string(request));
    const auto url=std::string(BAMBU_WIKI_URL)+"/v1/search";
    if(!http->Open("POST",url) || http->GetStatusCode()!=200){http->Close();return R"({"available":false,"reason":"wiki_backend_unreachable","sources":[]})";}
    std::string response;char buffer[512];
    while(response.size()<=16384){
        const int read=http->Read(buffer,sizeof(buffer));
        if(read<0){http->Close();return R"({"available":false,"reason":"wiki_backend_read_error","sources":[]})";}
        if(read==0)break;
        response.append(buffer,read);
    }
    http->Close();
    if(response.size()>16384)return R"({"available":false,"reason":"wiki_response_too_large","sources":[]})";
    auto root=cJSON_ParseWithLengthOpts(response.c_str(),response.size()+1,nullptr,true);
    if(!root || !cJSON_IsObject(root) || !cJSON_IsArray(cJSON_GetObjectItem(root,"sources"))){
        cJSON_Delete(root);return R"({"available":false,"reason":"wiki_response_invalid","sources":[]})";
    }
    cJSON_Delete(root);return response;
}
}
