#include "auth.h"
#include "mono_clock.h"
#include "platform.h"
#include "ui_view.h"
#include "input_model.h"
#include "language.h"
#include <orbis/_types/pad.h>
#include <algorithm>
#include <atomic>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

static uint32_t pressed(){return ui_input();}
static void release_buttons(){ui_input_sync();}
static uint64_t millis() { return mono_ms(); }
// Titles and notices are English at their use, translated here.
static void login_header(const std::string& title){app_header("","");text(400,28,T(title.c_str()),g_theme.text,24,800);}
static void login_notice(const std::string& title) { login_header(title); text(60, 180, T(title.c_str()), g_theme.text); present(); }
static void login_error() {
    UiInputScope input_scope;
    release_buttons();
    for (;;) {
        login_header("Unable to sign in"); wrapped(60, 170, last_error, g_theme.text, 7);
        hint_bar({{'C', "Return"}}); present();
        if (pressed() & ORBIS_PAD_BUTTON_CIRCLE) { release_buttons(); return; }
        usleep(16000);
    }
}
static void row(int y, const std::string& label, bool selected) {
    rect(48, y, 1184, 64, selected ? g_theme.panel_hi : g_theme.panel);
    if (selected) rect(48, y, 5, 64, g_theme.accent);
    text(70, y + 18, T(label.c_str()), selected ? g_theme.accent : g_theme.text, 24, 1130);
}

static bool server_screen(std::string& server) {
    int selected = 0;
    release_buttons();
    for (;;) {
        login_header("Connect to Jellyfin");
        text(60, 116, T("Enter the complete server URL, including its port."), g_theme.text_dim);
        const std::string labels[] = {TF("Server URL: %s", server.c_str()), "Connect", "Exit application"};
        for (int i = 0; i < 3; ++i) row(190 + i * 90, labels[i], i == selected);
        text(60, 515, TF("Example: %s", "http://192.168.1.100:8096"), g_theme.text_dim);
        text(60, 555, T("Keep any Jellyfin base path after the port, if your server uses one."), g_theme.text_dim);
        hint_bar({{'D', "Select"}, {'X', "Edit / connect"}, {'O', "Done (in keyboard)"}});
        present();
        const auto b = pressed();
        if (b & ORBIS_PAD_BUTTON_UP) selected = (selected + 2) % 3;
        if (b & ORBIS_PAD_BUTTON_DOWN) selected = (selected + 1) % 3;
        if (b & ORBIS_PAD_BUTTON_CROSS) {
            if (selected == 0) keyboard(T("Jellyfin server URL (including port)"), server, false, KeyboardType::Url);
            else if (selected == 1) {
                login_notice("Checking server...");
                if (connect_server(server)) { server = g_server; return true; }
                login_error();
            } else return false;
            release_buttons();
        }
        usleep(16000);
    }
}

static bool password_screen() {
    UiInputScope input_scope;
    int selected = 0;
    std::string username = g_username, password;
    release_buttons();
    for (;;) {
        login_header("Username and password"); text(60, 116, g_server, g_theme.text_dim);
        const std::string labels[] = {TF("Username: %s", username.c_str()), TF("Password: %s", std::string(password.size(), '*').c_str()), "Sign in", "Back"};
        for (int i = 0; i < 4; ++i) row(170 + i * 85, labels[i], i == selected);
        hint_bar({{'D', "Select"}, {'X', "Edit / sign in"}, {'C', "Sign-in choices"}});
        present(); const auto b = pressed();
        if (b & ORBIS_PAD_BUTTON_UP) selected = (selected + 3) % 4;
        if (b & ORBIS_PAD_BUTTON_DOWN) selected = (selected + 1) % 4;
        if ((b & ORBIS_PAD_BUTTON_CIRCLE) || ((b & ORBIS_PAD_BUTTON_CROSS) && selected == 3)) {
            std::fill(password.begin(), password.end(), 0); return false;
        }
        if (b & ORBIS_PAD_BUTTON_CROSS) {
            if (selected == 0) keyboard(T("Jellyfin username"), username);
            else if (selected == 1) keyboard(T("Jellyfin password"), password, true);
            else if (selected == 2) {
                if (username.empty() || username.size() >= sizeof g_username) last_error = T("Enter a valid Jellyfin username.");
                else {
                    login_notice("Signing in...");
                    if (login(username, password)) { std::fill(password.begin(), password.end(), 0); return true; }
                }
                login_error();
            }
            release_buttons();
        }
        usleep(16000);
    }
}

struct PollJob {
    QuickConnectSession session;
    QuickConnectStatus status = QuickConnectStatus::Pending;
    std::atomic<bool> finished{false};
};
static void* poll_worker(void* opaque) {
    auto* job = static_cast<PollJob*>(opaque);
    job->status = quick_connect_poll(job->session);
    job->finished = true;
    return nullptr;
}
static bool quick_connect_screen() {
    UiInputScope input_scope;
    QuickConnectSession session;
    login_notice("Starting Quick Connect...");
    if (!quick_connect_begin(session)) { login_error(); return false; }
    release_buttons();
    pthread_t worker{};
    PollJob job;
    job.session = session;
    bool polling = false;
    uint64_t next_poll = 0, deadline = millis() + 10 * 60 * 1000;
    auto finish_poll = [&] {
        if (polling) { pthread_join(worker, nullptr); polling = false; http_cancel_disarm(); }
    };
    auto cancel_poll = [&] { if (polling) http_cancel_fire(); finish_poll(); };
    for (;;) {
        login_header("Sign in with Quick Connect");
        text(60, 130, T("On your phone, tablet or computer, open Jellyfin and sign in."), g_theme.text);
        text(60, 180, T("Go to Settings > Quick Connect and enter this code:"), g_theme.text_dim);
        const int count = (int)session.code.size();
        const int box_width = count <= 8 ? 88 : 48, gap = 12;
        const int start = (WIDTH - count * (box_width + gap) + gap) / 2;
        for (int i = 0; i < count; ++i) {
            rect(start + i * (box_width + gap), 280, box_width, 96, g_theme.panel_hi);
            text(start + i * (box_width + gap) + box_width / 2 - 10, 309, std::string(1, session.code[i]), g_theme.accent, 32);
        }
        text(60, 425, TF("Open %s in your browser to approve this PS4.", g_server), g_theme.text_dim);
        text(60, 505, T("Waiting for approval") + std::string((millis() / 500) % 4, '.'), g_theme.accent);
        hint_bar({{'C', "Cancel and return to sign-in choices"}});
        present();
        if (pressed() & ORBIS_PAD_BUTTON_CIRCLE) { cancel_poll(); return false; }
        if (millis() >= deadline) {
            cancel_poll(); last_error = T("This Quick Connect code has timed out. Choose Quick Connect for a new code."); login_error(); return false;
        }
        if (polling && job.finished) {
            finish_poll();
            if (job.status == QuickConnectStatus::Approved) {
                login_notice("Completing sign-in...");
                if (quick_connect_finish(session)) return true;
                login_error(); return false;
            }
            if (job.status != QuickConnectStatus::Pending) { login_error(); return false; }
            next_poll = millis() + 3000;
        }
        if (!polling && millis() >= next_poll) {
            job.finished = false;
            http_cancel_arm();
            if (pthread_create(&worker, nullptr, poll_worker, &job) != 0) {
                http_cancel_disarm(); last_error = T("Unable to check Quick Connect approval. Try username and password."); login_error(); return false;
            }
            polling = true;
        }
        usleep(16000);
    }
}

bool sign_in() {
    UiInputScope input_scope;
    // Another user of this console shares their sign-in (Settings > Account):
    // use it, or go on to sign in with an account of your own.
    const std::string shared = shared_name();
    if (!shared.empty()) {
        int selected = 0;
        release_buttons();
        for (;;) {
            login_header("Sign in");
            row(195, std::string(T("Use the shared sign-in")) + " (" + shared + ")", selected == 0);
            row(300, "Sign in with my own account", selected == 1);
            hint_bar({{'D', "Select"}, {'X', "Continue"}});
            present(); const auto b = pressed();
            if (b & (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN)) selected = 1 - selected;
            if (b & ORBIS_PAD_BUTTON_CROSS) { release_buttons(); if (selected == 0 && share_use()) return true; break; }
            usleep(16000);
        }
    }
    std::string server = g_server[0] ? g_server : "http://";
    for (;;) {
        if (!server_screen(server)) return false;
        int selected = 0;
        release_buttons();
        for (;;) {
            login_header("Choose how to sign in");
            text(60, 116, g_server, g_theme.text_dim);
            row(195, "Quick Connect - approve this PS4 from another device", selected == 0);
            row(300, "Username and password", selected == 1);
            row(405, "Change server", selected == 2);
            hint_bar({{'D', "Select"}, {'X', "Continue"}, {'S', "Password"}, {'C', "Change server"}});
            present(); const auto b = pressed();
            if (b & ORBIS_PAD_BUTTON_UP) selected = (selected + 2) % 3;
            if (b & ORBIS_PAD_BUTTON_DOWN) selected = (selected + 1) % 3;
            if ((b & ORBIS_PAD_BUTTON_CIRCLE) || ((b & ORBIS_PAD_BUTTON_CROSS) && selected == 2)) break;
            if (b & ORBIS_PAD_BUTTON_SQUARE) { if (password_screen()) return true; release_buttons(); }
            else if (b & ORBIS_PAD_BUTTON_CROSS) {
                if (selected == 0 ? quick_connect_screen() : password_screen()) return true;
                release_buttons();
            }
            usleep(16000);
        }
    }
}
