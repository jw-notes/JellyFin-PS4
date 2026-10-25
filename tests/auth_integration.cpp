#include "auth.h"
#include <cassert>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void endpoints() {
    ServerEndpoint endpoint;
    std::string url;
    assert(split_server_endpoint("192.168.1.10:8096", endpoint));
    assert(endpoint.address == "http://192.168.1.10" && endpoint.port == "8096");
    assert(build_server_url(endpoint.address, endpoint.port, url) && url == "http://192.168.1.10:8096");
    assert(build_server_url("https://example.test/jellyfin/", "443", url) && url == "https://example.test:443/jellyfin");
    assert(build_server_url("https://example.test/jellyfin/", "", url) && url == "https://example.test/jellyfin");
    assert(split_server_endpoint("http://[2001:db8::1]:8096/base/", endpoint));
    assert(endpoint.address == "http://[2001:db8::1]/base" && endpoint.port == "8096");
    assert(build_server_url(endpoint.address, endpoint.port, url) && url == "http://[2001:db8::1]:8096/base");
    assert(!build_server_url("http://example.test:8096", "8920", url));
    for (const char* port : {"0", "65536", "abc", "-1", "80/path"}) assert(!build_server_url("http://example.test", port, url));
    for (const char* address : {"", "ftp://host", "http://", "http://user:pass@host", "http://host?token=test", "http://host/path#fragment", "http://host\r\nHeader:value", "http://host:", "http://[bad]"}) assert(!build_server_url(address, "8096", url));
    puts("PASS server URL/port parsing: hostnames, embedded ports, HTTPS base paths, IPv6 and invalid input");
}
int main(int argc, char** argv) {
    endpoints();
    if(argc==3 && std::string(argv[1])=="--probe"){
        assert(storage_init(44));
        assert(connect_server(argv[2]));
        printf("PASS complete server URL probe: %s\n",g_server);
        return 0;
    }
    if (argc < 4) return 0;
    assert(storage_init(43));
    ServerEndpoint endpoint;
    assert(split_server_endpoint(argv[1], endpoint));
    assert(connect_server(argv[1]));
    assert(!g_token[0] && !g_userid[0]);
    assert(login(argv[2], argv[3]));
    const std::string user_id = g_userid;
    // Temporarily disable Quick Connect on the isolated real server, then restore
    // the full original configuration before checking the result.
    assert(http_request(HTTP_GET, (std::string(g_server)+"/System/Configuration").c_str(), "", g_token, responseBuffer, RESPONSE_SIZE) == 200);
    const std::string original_config = responseBuffer;
    const char* enabled = find_top_value(responseBuffer, responseBuffer+strlen(responseBuffer), "QuickConnectAvailable");
    assert(enabled);
    std::string disabled = original_config;
    const size_t offset = enabled-responseBuffer;
    const size_t length = !strncmp(enabled,"true",4)?4:5;
    disabled.replace(offset,length,"false");
    char response[4096];
    assert(http_request(HTTP_POST,(std::string(g_server)+"/System/Configuration").c_str(),disabled.c_str(),g_token,response,sizeof response)==204);
    QuickConnectSession session;
    const bool accepted_disabled = quick_connect_begin(session);
    const std::string disabled_message = last_error;
    const int restored = http_request(HTTP_POST,(std::string(g_server)+"/System/Configuration").c_str(),original_config.c_str(),g_token,response,sizeof response);
    assert(restored==204 && !accepted_disabled && disabled_message.find("disabled")!=std::string::npos && !g_auth_expired);
    assert(quick_connect_begin(session));
    assert(session.code.size()==6 && !session.secret.empty());
    assert(quick_connect_poll(session)==QuickConnectStatus::Pending);
    assert(!quick_connect_finish(session)); // A code cannot be exchanged before approval.
    assert(!g_auth_expired); // Unauthenticated Quick Connect errors must not expire saved logins.
    auto discarded=session;
    discarded.secret="invalid-or-expired-quick-connect-secret";
    assert(quick_connect_poll(discarded)==QuickConnectStatus::Expired);
    assert(!g_auth_expired);
    assert(http_request(HTTP_POST,(std::string(g_server)+"/QuickConnect/Authorize?code="+encode(session.code)).c_str(),"",g_token,response,sizeof response)==200);
    assert(quick_connect_poll(session)==QuickConnectStatus::Approved);
    assert(quick_connect_finish(session));
    assert(g_userid==user_id && std::string(g_username)==argv[2]);
    const std::string token=g_token;
    g_token[0]=g_userid[0]=g_username[0]=0;
    assert(load_session() && g_token==token && g_userid==user_id && std::string(g_username)==argv[2]);
    Page libraries;
    assert(fetch_page("",0,"",0,libraries) && !libraries.items.empty());
    app_shutdown();
    logout();
    puts("PASS real Jellyfin: connect server first, password fallback, Quick Connect disabled, pending, unapproved exchange, expired code, approval, token exchange, saved session and library access");
}
