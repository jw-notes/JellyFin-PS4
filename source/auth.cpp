#include "auth.h"
#include "json_unescape.h"
#include <algorithm>
#include <ctype.h>
#include <string.h>

static bool valid_port(const std::string& value) {
    if (value.empty()) return true;
    if (value.size() > 5) return false;
    unsigned port = 0;
    for (unsigned char c : value) {
        if (c < '0' || c > '9') return false;
        port = port * 10 + c - '0';
    }
    return port > 0 && port <= 65535;
}

bool split_server_endpoint(const std::string& input, ServerEndpoint& endpoint) {
    std::string value = input;
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) { last_error = "Enter your Jellyfin server address."; return false; }
    value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    if (value.find("://") == std::string::npos) value = "http://" + value;
    const auto scheme_end = value.find("://");
    std::string scheme = value.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return (char)tolower(c); });
    if (scheme != "http" && scheme != "https") { last_error = "Use an http:// or https:// server address."; return false; }
    const auto authority_start = scheme_end + 3;
    auto path_start = value.find('/', authority_start);
    const auto authority = value.substr(authority_start, path_start == std::string::npos ? path_start : path_start - authority_start);
    std::string path = path_start == std::string::npos ? "" : value.substr(path_start);
    if (authority.empty() || value.find_first_of("\r\n\t ?#\\") != std::string::npos || authority.find('@') != std::string::npos) {
        last_error = "Enter a server hostname or IP address, without credentials or a query string."; return false;
    }
    std::string host, port;
    if (authority[0] == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos || close == 1) { last_error = "IPv6 addresses must be enclosed in brackets."; return false; }
        host = authority.substr(0, close + 1);
        for (size_t i = 1; i < close; ++i) {
            const unsigned char c = authority[i];
            if (!isxdigit(c) && c != ':' && c != '.') { last_error = "Invalid IPv6 server address."; return false; }
        }
        if (host.find(':') == std::string::npos) { last_error = "Invalid IPv6 server address."; return false; }
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') { last_error = "Invalid characters after the server address."; return false; }
            port = authority.substr(close + 2);
            if (port.empty()) { last_error = "Enter a port number after the colon."; return false; }
        }
    } else {
        const auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != std::string::npos) {
            port = authority.substr(colon + 1);
            if (port.empty()) { last_error = "Enter a port number after the colon."; return false; }
        }
        for (unsigned char c : host) if (!isalnum(c) && c != '-' && c != '_' && c != '.') {
            last_error = "Invalid server hostname. Enclose IPv6 addresses in brackets."; return false;
        }
    }
    if (host.empty() || !valid_port(port)) { last_error = "The server port must be a number between 1 and 65535."; return false; }
    while (!path.empty() && path.back() == '/') path.pop_back();
    endpoint.address = scheme + "://" + host + path;
    endpoint.port = port;
    return true;
}

bool build_server_url(const std::string& address, const std::string& port, std::string& url) {
    ServerEndpoint endpoint;
    if (!split_server_endpoint(address, endpoint)) return false;
    if (!valid_port(port)) { last_error = "The server port must be a number between 1 and 65535."; return false; }
    const std::string chosen_port = port.empty() ? endpoint.port : port;
    if (!port.empty() && !endpoint.port.empty() && port != endpoint.port) {
        last_error = "The URL and port field specify different ports. Enter the address without a port."; return false;
    }
    auto path = endpoint.address.find('/', endpoint.address.find("://") + 3);
    url = endpoint.address;
    if (!chosen_port.empty()) url.insert(path == std::string::npos ? url.size() : path, ":" + chosen_port);
    if (url.size() >= sizeof g_server) { last_error = "The server address is too long."; return false; }
    return true;
}

bool connect_server(const std::string& address, const std::string& port) {
    std::string server;
    if (!build_server_url(address, port, server)) return false;
    plog(("Connecting to Jellyfin: " + server).c_str());
    const int status = http_request(HTTP_GET, (server + "/System/Info/Public").c_str(), "", nullptr, responseBuffer, RESPONSE_SIZE);
    if (status != 200) {
        last_error = status < 0 ? "Could not reach " + server + ". " + http_failure_detail() : "Jellyfin returned HTTP " + std::to_string(status) + ". Check the full URL, port and base path."; return false;
    }
    char id[128], name[128];
    if (!json_get_string(responseBuffer, "Id", id, sizeof id) || !id[0] ||
        !json_get_string(responseBuffer, "ServerName", name, sizeof name)) {
        last_error = "This address did not return a Jellyfin server. Check the address, port and base path."; return false;
    }
    const bool changed = server != g_server;
    snprintf(g_server, sizeof g_server, "%s", server.c_str());
    g_token[0] = g_userid[0] = 0;
    if (changed) g_username[0] = 0;
    g_auth_expired = false;
    if (!save_session()) plog("Connected server could not be saved");
    return true;
}

bool connect_server(const std::string& complete_url) { return connect_server(complete_url, ""); }

static int quick_request(int method, const std::string& path, const std::string& body, char* response, int cap) {
    return http_request(method, (std::string(g_server) + path).c_str(), body.c_str(), nullptr, response, cap);
}

bool quick_connect_begin(QuickConnectSession& session) {
    session = {};
    char response[4096];
    int status = quick_request(HTTP_GET, "/QuickConnect/Enabled", "", response, sizeof response);
    if (status != 200) { last_error = "Could not check Quick Connect (HTTP " + std::to_string(status) + "). Try username and password."; return false; }
    const char* value = skip_ws(response, response + strlen(response));
    if (!read_json_bool(value, response + strlen(response))) {
        last_error = "Quick Connect is disabled on this server. Enable it in Jellyfin or choose username and password."; return false;
    }
    status = quick_request(HTTP_POST, "/QuickConnect/Initiate", "{}", response, sizeof response);
    if (status != 200) { last_error = "Could not start Quick Connect (HTTP " + std::to_string(status) + ")."; return false; }
    char code[32], secret[256];
    if (!json_get_string(response, "Code", code, sizeof code) || !code[0] ||
        !json_get_string(response, "Secret", secret, sizeof secret) || !secret[0]) {
        last_error = "The server returned an invalid Quick Connect response."; return false;
    }
    session.code = code;
    session.secret = secret;
    return true;
}

QuickConnectStatus quick_connect_poll(const QuickConnectSession& session) {
    if (session.secret.empty()) { last_error = "The Quick Connect session has ended."; return QuickConnectStatus::Expired; }
    char response[4096];
    const int status = quick_request(HTTP_GET, "/QuickConnect/Connect?secret=" + encode(session.secret), "", response, sizeof response);
    if (status == 404 || status == 401 || status == 410) {
        last_error = "This Quick Connect code has expired or was revoked. Choose Quick Connect to get a new code.";
        return QuickConnectStatus::Expired;
    }
    if (status != 200) { last_error = "Could not check Quick Connect approval (HTTP " + std::to_string(status) + ")."; return QuickConnectStatus::Failed; }
    const char* end = response + strlen(response);
    const char* approved = find_top_value(response, end, "Authenticated");
    if (!approved) { last_error = "The server returned an invalid Quick Connect status."; return QuickConnectStatus::Failed; }
    return read_json_bool(approved, end) ? QuickConnectStatus::Approved : QuickConnectStatus::Pending;
}

bool quick_connect_finish(const QuickConnectSession& session) {
    if (session.secret.empty()) { last_error = "The Quick Connect session has ended."; return false; }
    const int status = quick_request(HTTP_POST, "/Users/AuthenticateWithQuickConnect", "{\"Secret\":" + json_quote(session.secret) + "}", responseBuffer, RESPONSE_SIZE);
    if (status != 200) { last_error = "Quick Connect sign-in was refused (HTTP " + std::to_string(status) + "). Try a new code."; return false; }
    return finish_login(responseBuffer);
}
