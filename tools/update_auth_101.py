from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/app.cpp';t=f.read_text().replace('#include "app.h"','#include "app.h"\n#include "auth.h"')
a=t.index('bool login(');b=t.index('\nvoid logout()',a)
t=t[:a]+'''bool finish_login(const char* response, const std::string& fallback_username) {
    const auto token = field(response, "AccessToken");
    const char* end = response + strlen(response);
    const char* user = find_top_value(response, end, "User");
    const char* user_end = user ? object_end(user, end) : nullptr;
    const std::string user_json = user_end ? std::string(user, user_end) : "";
    const auto uid = field(user_json.c_str(), "Id");
    auto username = field(user_json.c_str(), "Name");
    if (username.empty()) username = fallback_username;
    if (token.empty() || uid.empty() || token.size() >= sizeof g_token || uid.size() >= sizeof g_userid || username.size() >= sizeof g_username) {
        last_error = "Invalid authentication response"; return false;
    }
    snprintf(g_token, sizeof g_token, "%s", token.c_str());
    snprintf(g_userid, sizeof g_userid, "%s", uid.c_str());
    snprintf(g_username, sizeof g_username, "%s", username.c_str());
    g_auth_expired = false;
    if (!save_session()) plog("Signed in; session could not be saved");
    return true;
}
bool login(const std::string& username, const std::string& password) {
    g_token[0] = g_userid[0] = 0;
    int status = request(HTTP_POST, "/Users/AuthenticateByName", "{\\\"Username\\\":" + json_quote(username) + ",\\\"Pw\\\":" + json_quote(password) + "}");
    return status == 200 && finish_login(responseBuffer, username);
}'''+t[b:]
t += '\nvoid app_shutdown(){ flush_reports(); plog("Application workers stopped"); }\n'
f.write_text(t,newline='\n')
f=p/'source/app.h';t=f.read_text().replace('void logout();','void logout();\nvoid app_shutdown();');f.write_text(t,newline='\n')
