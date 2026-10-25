from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4/tests/keyboard_test.cpp')
s=p.read_text().replace('for(auto& k:rows[0])numbers+=k.value;for(auto& k:rows[1])qwerty+=k.value;for(auto& k:rows[2])home+=k.value;', 'for(auto& k:rows[0])numbers+=k.value;\n    for(auto& k:rows[1])qwerty+=k.value;\n    for(auto& k:rows[2])home+=k.value;')
p.write_text(s,newline='\n')
