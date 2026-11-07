// Leaving the sandbox with GoldHEN's own escape.
//
// GoldHEN 2.3 and later answer syscall 500 ("SDK commands", the interface of
// the GoldHEN Plugins SDK, MIT, vendored in vendor/goldhen_sdk). Command 2,
// sys_sdk_jailbreak, saves the process's credentials, prison and root / jail /
// current directories into a jailbreak_backup and replaces them with the
// system's; command 3, sys_sdk_unjailbreak, writes a backup back.
//
// The jailbreak makes the process root. Only the escaped prison and root
// directory are needed, so it drops back to its own uid, gid and groups at
// once: files it writes keep the user's ownership.
#include "sandbox_ps4.h"
#include "app.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <vector>

extern "C" {
#include "../vendor/goldhen_sdk/include/GoldHEN.h"
int orbis_syscall(int num, ...);  // vendor/goldhen_sdk/source/Syscall.c
}

namespace {

// The console's kernel is FreeBSD: its SIGSYS is 12 (the toolchain's
// <signal.h> has Linux numbers), and libkernel's sigaction takes FreeBSD's
// structure. A syscall the kernel does not have raises SIGSYS, which would end
// the app: it is ignored while GoldHEN is asked (it then fails with ENOSYS).
constexpr int kSigSys = 12;
constexpr int kSysSetgid = 181;  // musl's setgid is an ENOSYS stub here
struct FreeBsdSigaction { void (*handler)(int); int flags; uint32_t mask[4]; };
extern "C" int fbsd_sigaction(int sig, const FreeBsdSigaction* act, FreeBsdSigaction* old) __asm__("sigaction");
class IgnoreSigSys {
public:
    IgnoreSigSys() { FreeBsdSigaction ignore{}; ignore.handler = (void (*)(int))1; saved_ = fbsd_sigaction(kSigSys, &ignore, &old_) == 0; }
    ~IgnoreSigSys() { if (saved_) fbsd_sigaction(kSigSys, &old_, nullptr); }
private:
    FreeBsdSigaction old_{};
    bool saved_ = false;
};

jailbreak_backup g_backup{};
bool g_lifted = false;
uid_t g_uid = 0;
gid_t g_gid = 0;
std::vector<gid_t> g_groups;

void log_identity(const char* when) {
    char line[160];
    snprintf(line, sizeof line, "USB (%s): uid %u euid %u gid %u egid %u, in sandbox %d", when, (unsigned)getuid(),
             (unsigned)geteuid(), (unsigned)getgid(), (unsigned)getegid(), sceKernelIsInSandbox());
    plog(line);
}

// Back to the app's own groups, gid and uid (in that order: each needs root).
bool drop_root() {
    if (geteuid() != 0) return true;
    if (g_uid == 0) { plog("USB: the app itself runs as uid 0; nothing to drop"); return true; }
    bool ok = true;
    char line[96];
    if (!g_groups.empty() && setgroups(g_groups.size(), g_groups.data()) != 0) {
        snprintf(line, sizeof line, "USB: setgroups failed (errno %d)", errno); plog(line); ok = false;
    }
    {
        IgnoreSigSys guard;
        if (orbis_syscall(kSysSetgid, (int)g_gid) != 0) {
            snprintf(line, sizeof line, "USB: setgid(%u) failed (errno %d)", (unsigned)g_gid, errno); plog(line); ok = false;
        }
    }
    if (setuid(g_uid) != 0) {
        snprintf(line, sizeof line, "USB: setuid(%u) failed (errno %d)", (unsigned)g_uid, errno); plog(line); ok = false;
    }
    log_identity(ok ? "root dropped" : "root NOT fully dropped");
    return ok;
}

}  // namespace

namespace sandbox {

bool lifted() { return g_lifted; }

bool leave() {
    char line[200];
    if (sceKernelIsInSandbox() == 0) { plog("USB: already outside the sandbox"); return true; }
    uint32_t version;
    int err;
    {
        IgnoreSigSys guard;
        errno = 0;
        version = sys_sdk_version();
        err = errno;
    }
    if (version == 0 || version == 0xFFFFFFFFu) {
        snprintf(line, sizeof line, "USB: GoldHEN's SDK does not answer (syscall 500, errno %d): GoldHEN 2.3 or later is needed for USB drives", err);
        plog(line);
        return false;
    }
    snprintf(line, sizeof line, "USB: GoldHEN SDK %u.%02x", (unsigned)(version >> 8), (unsigned)(version & 0xFF));
    plog(line);
    g_uid = getuid();
    g_gid = getgid();
    const int count = getgroups(0, nullptr);
    if (count > 0) { g_groups.resize(count); if (getgroups(count, g_groups.data()) != count) g_groups.clear(); }
    log_identity("before");
    int rc;
    {
        IgnoreSigSys guard;
        errno = 0;
        rc = sys_sdk_jailbreak(&g_backup);
        err = errno;
    }
    if (rc != 0) {
        snprintf(line, sizeof line, "USB: GoldHEN jailbreak failed (rc %d, errno %d)", rc, err);
        plog(line);
        return false;
    }
    g_lifted = true;
    log_identity("jailbroken");
    drop_root();
    const bool out = sceKernelIsInSandbox() == 0;
    plog(out ? "USB: leaving the sandbox with the GoldHEN SDK succeeded" : "USB: leaving the sandbox with the GoldHEN SDK failed");
    return out;
}

void restore() {
    if (!g_lifted) return;
    int rc;
    {
        IgnoreSigSys guard;
        rc = sys_sdk_unjailbreak(&g_backup);
    }
    g_lifted = false;
    char line[64];
    snprintf(line, sizeof line, "USB: GoldHEN unjailbreak rc %d", rc);
    plog(line);
}

}  // namespace sandbox
