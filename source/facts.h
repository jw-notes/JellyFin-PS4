#pragma once
#include "app.h"
void facts_request(const Item& item);
bool facts_copy(const std::string& id,Item& item,std::string& status);
void facts_stop(bool clear_cache=true);
