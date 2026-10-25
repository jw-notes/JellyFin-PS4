from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/ui_view.h';t=f.read_text().replace('const std::string& tab="Home");','const std::string& tab="Home",int focused_tab=-1);').replace('int selected,bool home);','int selected,bool home,int focused_tab=-1);');f.write_text(t,newline='\n')
f=p/'source/ui_view.cpp';t=f.read_text().replace('void app_header(const std::string& title,const std::string& tab){','void app_header(const std::string& title,const std::string& tab,int focused_tab){').replace('for(auto name:tabs){if(tab.empty())break;bool active=tab==name;', 'int tab_index=0;for(auto name:tabs){if(tab.empty())break;bool active=tab==name;if(tab_index++==focused_tab){rect(x-10,18,112,48,g_theme.panel_hi);outline(x-10,18,112,48,g_theme.focus_ring,2);}')
t=t.replace('int selected,bool home){','int selected,bool home,int focused_tab){').replace('title.rfind("Search:",0)==0?"Search":"Home");','title.rfind("Search:",0)==0?"Search":"Home",focused_tab);')
a=t.index(' const std::string labels[]=',t.index('void draw_detail'));b=t.index('\n text(48,667',a)
t=t[:a]+''' const std::string labels[]={item.resume&&settings.resume?"Resume playback":"Play","Version: "+version,"Audio: "+audio,"Subtitles: "+subtitles,item.favourite?"Remove from favourites":"Add to favourites",item.played?"Mark unwatched":"Mark watched"};
 for(int i=0;i<6;++i){int y=324+i*52;bool focus=selected==i;rect(340,y,892,45,focus?(i==0?g_theme.accent:g_theme.panel_hi):g_theme.panel);if(focus)outline(338,y-2,896,49,g_theme.focus_ring,2);text(358,y+10,fit_text(labels[i],850),focus&&i==0?g_theme.bg:focus?g_theme.text:g_theme.text_dim,24,850);}
'''+t[b:]
f.write_text(t,newline='\n')
f=p/'source/main.cpp';t=f.read_text().replace('sources.n_sources?sources.source[source].id:"");return;', 'sources.n_sources?sources.source[source].id:"");wait_release();continue;');f.write_text(t,newline='\n')
