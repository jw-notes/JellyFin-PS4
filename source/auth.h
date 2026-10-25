#pragma once
#include "app.h"
struct ServerEndpoint {
    std::string address;
    std::string port;
};
bool split_server_endpoint(const std::string& input, ServerEndpoint& endpoint);
bool build_server_url(const std::string& address, const std::string& port, std::string& url);
bool connect_server(const std::string& address, const std::string& port);
bool connect_server(const std::string& complete_url);
bool finish_login(const char* response, const std::string& fallback_username = "");
struct QuickConnectSession { std::string code, secret; };
enum class QuickConnectStatus { Pending, Approved, Expired, Failed };
bool quick_connect_begin(QuickConnectSession& session);
QuickConnectStatus quick_connect_poll(const QuickConnectSession& session);
bool quick_connect_finish(const QuickConnectSession& session);

bool sign_in();
