from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/platform_ps4.cpp';t=f.read_text()
a=t.index('#define STB_TRUETYPE_IMPLEMENTATION');b=t.index('static int video=',a);font_includes=t[a:b];t=t[:a]+t[b:]
a=t.index('static stbtt_fontinfo');b=t.index('int current_user()',a);font_state=t[a:b];t=t[:a]+t[b:]
a=t.index(' stbtt_InitFont(');b=t.index('\n return true;',a);font_init=t[a:b];t=t[:a]+' render_init();'+t[b:]
a=t.index('void clear(');drawing=t[a:];t=t[:a];f.write_text(t)
render='#include "app.h"\n#include "platform.h"\n#include <algorithm>\n#include <map>\n#include <unistd.h>\n'+font_includes+font_state+'void render_init(){\n'+font_init+'\n}\n'+drawing
(p/'source/render.cpp').write_text(render)
f=p/'source/platform.h';t=f.read_text().replace('bool platform_init();','void render_init();\nbool platform_init();');f.write_text(t)
f=p/'tools/build.sh';t=f.read_text().replace('source/core.cpp source/art.cpp','source/core.cpp source/render.cpp source/art.cpp');f.write_text(t,newline='\n')
