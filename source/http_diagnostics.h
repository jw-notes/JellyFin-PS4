#pragma once
#include <string>
// A diagnostic target must never include credentials or query-string secrets.
inline std::string http_log_target(const char* value){
    std::string url=value?value:"";
    auto end=url.find_first_of("?#");if(end!=std::string::npos)url.resize(end);
    auto scheme=url.find("://");
    if(scheme!=std::string::npos){auto authority=scheme+3;auto slash=url.find('/',authority);auto at=url.find('@',authority);if(at!=std::string::npos&&(slash==std::string::npos||at<slash))url.erase(authority,at-authority+1);}
    for(char& c:url)if((unsigned char)c<32||c==127)c=' ';
    if(url.size()>512)url.resize(512);
    return url;
}
