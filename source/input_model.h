#pragma once
#include <stdint.h>
#include <array>
class UiInputFilter {
    uint32_t held=0,nav=0;
    uint64_t repeat_at=0;
    std::array<uint64_t,24> released{};
public:
    void sync(uint32_t raw,uint64_t now);
    uint32_t update(uint32_t raw,uint64_t now);
};
void ui_input_sync();
uint32_t ui_input();
class UiInputScope {
public:
    UiInputScope(){ui_input_sync();}
    ~UiInputScope(){ui_input_sync();}
};
