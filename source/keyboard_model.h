#pragma once
#include <string>
#include <vector>
enum class KeyboardType { Text, Url };
enum class KeyKind { Character, Caps, Symbols, Backspace, Space, Done, Scheme };
struct KeyboardKey { KeyKind kind; std::string label, value; int units; };
using KeyboardRow = std::vector<KeyboardKey>;
using KeyboardRows = std::vector<KeyboardRow>;
// Layout adapted from the original PS3 osk_build(): number row, QWERTY,
// home row, Caps/Z row, symbols page and wide bottom action keys.
KeyboardRows keyboard_layout(bool symbols, bool caps, KeyboardType type);
class KeyboardEditor {
public:
    explicit KeyboardEditor(const std::string& value, KeyboardType type = KeyboardType::Text);
    std::string input;
    size_t cursor;
    bool caps = false, symbols = false;
    int row = 1, column = 0;
    KeyboardType type;
    KeyboardRows layout() const;
    void move(int horizontal, int vertical);
    void move_cursor(int direction);
    void backspace();
    void insert(const std::string& value);
    void toggle_symbols();
    bool activate(); // true when Done is selected
    std::string displayed(bool password) const;
};
