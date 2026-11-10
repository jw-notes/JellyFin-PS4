// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
#include "app.h"
void facts_request(const Item& item);
bool facts_copy(const std::string& id,Item& item,std::string& status);
void facts_stop(bool clear_cache=true);
