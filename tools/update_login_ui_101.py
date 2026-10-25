from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/auth.h';t=f.read_text()+'\nbool sign_in();\n';f.write_text(t,newline='\n')
f=p/'source/main.cpp';t=f.read_text().replace('#include "app.h"','#include "app.h"\n#include "auth.h"');a=t.index('static bool sign_in()');b=t.index('static void play(',a);t=t[:a]+t[b:];t=t.replace('int main(){if(!platform_init())return 1;', 'int main(){if(!platform_init()){platform_shutdown();platform_exit(1);}')
t=t.replace('error_screen();platform_shutdown();return 2;', 'error_screen();app_shutdown();http_end();platform_shutdown();platform_exit(2);')
t=t.replace('}http_end();platform_shutdown();return 0;}', '}player_close();app_shutdown();http_end();platform_shutdown();platform_exit(0);}')
f.write_text(t,newline='\n')
