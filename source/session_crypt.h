// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// The saved sign-in's access token, sealed to this console: AES-256-GCM with a
// key made from the console's own OpenPSID. A session file copied off the
// console (over FTP, or in a backup) is no use anywhere else. It does not keep
// the token from code running on this same console, which can make the key too.
#pragma once
#include <string>
// "v1:" and base64 of nonce, ciphertext and tag; empty when it could not be sealed.
std::string seal_token(const std::string& token);
// The token, or false: not sealed here, or tampered with.
bool open_token(const std::string& sealed,std::string& token);
