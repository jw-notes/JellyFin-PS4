from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/platform.h';t=f.read_text().replace('#include <string>','#include <string>\n#include "keyboard_model.h"').replace('void text(int x,int y,const std::string&,uint32_t color,int size=24,int maxwidth=1180);','int text_width(const std::string&,int size=24);\nvoid text(int x,int y,const std::string&,uint32_t color,int size=24,int maxwidth=1180);').replace('bool keyboard(const std::string& title,std::string& value,bool password=false);','bool keyboard(const std::string& title,std::string& value,bool password=false,KeyboardType type=KeyboardType::Text);');f.write_text(t,newline='\n')
f=p/'source/render.cpp';t=f.read_text();t=t[:t.index('bool keyboard(')];a=t.index('void clear(')
t=t[:a]+'''int text_width(const std::string& value,int size){
 int result=0,tier=size>=30;
 for(size_t i=0;i<value.size();i++){
  unsigned cp=(unsigned char)value[i];
  if(cp>=128){unsigned extra=(cp&0xe0)==0xc0?1:(cp&0xf0)==0xe0?2:(cp&0xf8)==0xf0?3:0;cp&=(1u<<(6-extra))-1;while(extra--&&i+1<value.size())cp=(cp<<6)|((unsigned char)value[++i]&63);}
  if(cp<32)continue;
  if(cp<128)result+=glyphs[tier][cp].advance;
  else{auto f=stbtt_FindGlyphIndex(&font,cp)?&font:&japanese_font;int advance;stbtt_GetCodepointHMetrics(f,cp,&advance,nullptr);result+=(int)(advance*stbtt_ScaleForPixelHeight(f,tier?32:24));}
 }
 return result;
}
'''+t[a:];f.write_text(t,newline='\n')
f=p/'source/login_ui.cpp';t=f.read_text();a=t.index('static bool server_screen(');b=t.index('\nstatic bool password_screen()',a)
t=t[:a]+'''static bool server_screen(std::string& server) {
    int selected = 0;
    release_buttons();
    for (;;) {
        login_header("Connect to Jellyfin");
        text(60, 116, "Enter the complete server URL, including its port.", g_theme.text_dim);
        const std::string labels[] = {"Server URL: " + server, "Connect", "Exit application"};
        for (int i = 0; i < 3; ++i) row(190 + i * 90, labels[i], i == selected);
        text(60, 515, "Example: http://192.168.1.100:8096", g_theme.text_dim);
        text(60, 555, "Keep any Jellyfin base path after the port, if your server uses one.", g_theme.text_dim);
        text(60, 650, "Up / Down: select   X: edit or connect   Options in keyboard: Done", g_theme.text_dim);
        present();
        const auto b = pressed();
        if (b & ORBIS_PAD_BUTTON_UP) selected = (selected + 2) % 3;
        if (b & ORBIS_PAD_BUTTON_DOWN) selected = (selected + 1) % 3;
        if (b & ORBIS_PAD_BUTTON_CROSS) {
            if (selected == 0) keyboard("Jellyfin server URL (including port)", server, false, KeyboardType::Url);
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
'''+t[b:]
t=t.replace('ServerEndpoint endpoint{"", "8096"};\n    if (g_server[0]) split_server_endpoint(g_server, endpoint);','std::string server = g_server[0] ? g_server : "http://";').replace('server_screen(endpoint)','server_screen(server)');f.write_text(t,newline='\n')
f=p/'source/auth.h';t=f.read_text().replace('bool connect_server(const std::string& address, const std::string& port);','bool connect_server(const std::string& address, const std::string& port);\nbool connect_server(const std::string& complete_url);');f.write_text(t,newline='\n')
f=p/'source/auth.cpp';t=f.read_text();t=t.replace('    const int status = http_request(HTTP_GET, (server + "/System/Info/Public")', '    plog(("Connecting to Jellyfin: " + server).c_str());\n    const int status = http_request(HTTP_GET, (server + "/System/Info/Public")')
t=t.replace('last_error = "Could not connect to Jellyfin (HTTP " + std::to_string(status) + "). Check the address and port.";', 'last_error = status < 0 ? "Could not reach " + server + ". " + http_failure_detail() : "Jellyfin returned HTTP " + std::to_string(status) + ". Check the full URL, port and base path.";')
t=t.replace('\nstatic int quick_request(', '\nbool connect_server(const std::string& complete_url) { return connect_server(complete_url, ""); }\n\nstatic int quick_request(');f.write_text(t,newline='\n')
f=p/'source/app.h';t=f.read_text().replace('void app_shutdown();','void app_shutdown();\nstd::string http_failure_detail();');f.write_text(t,newline='\n')
f=p/'tools/build.sh';t=f.read_text().replace('source/core.cpp source/auth.cpp','source/core.cpp source/keyboard_model.cpp source/keyboard_view.cpp source/keyboard.cpp source/auth.cpp').replace('-lSceNet -lSceHttp','-lSceNet -lSceNetCtl -lSceHttp');f.write_text(t,newline='\n')
f=p/'tools/test_renderer.sh';t=f.read_text().replace('source/render.cpp source/app.cpp','source/render.cpp source/keyboard_model.cpp source/keyboard_view.cpp source/keyboard.cpp source/app.cpp');f.write_text(t,newline='\n')
f=p/'version.json';f.write_text('{"app_version":"01.02","release_directory":"v1.02","client_version":"0.3.0"}\n',newline='\n')
