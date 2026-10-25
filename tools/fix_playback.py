from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
for name in ['source/app.h','source/app.cpp']:
 f=p/name;t=f.read_text().replace('int sub, int source,','int sub, const std::string& selected_source,').replace('int sub,int source,','int sub,const std::string& selected_source,')
 if name.endswith('.cpp'):
  t=t.replace('std::string body="{\\\"UserId', 'std::string body="{\\\"MediaSourceId\\\":"+json_quote(selected_source)+",\\\"UserId')
  t=t.replace('if(n++==source){obj.assign(a,b);break;}', 'n++; std::string candidate(a,b);if(field(candidate.c_str(),"Id")== (selected_source.empty()?std::string(sources.source[0].id):selected_source)){obj=candidate;break;}')
  t=t.replace('\\\"AudioStreamIndex\\\":', '\\\"AudioStreamIndex\\\":')
  t=t.replace('else if(url[0]==\'/\')url=std::string(g_server)+url;', 'else if(url[0]==\'/\')url=std::string(g_server)+url;\n if(!settings.direct){url += (url.find(\'?\')==std::string::npos?"?":"&"); url += "MaxWidth=1920&MaxHeight=1080&AudioSampleRate=48000&MaxAudioChannels=2";}')
 f.write_text(t)
f=p/'source/main.cpp';t=f.read_text().replace('int subtitle,int source)', 'int subtitle,const std::string& source)').replace('tracks->subs[sub_slot].index:-1,source);','tracks->subs[sub_slot].index:-1,sources.n_sources?sources.source[source].id:"");')
t=t.replace('detail_screen(item);reload=true;}}','detail_screen(item);reload=true;}continue;}').replace('stack.pop_back();reload=true;}}','stack.pop_back();reload=true;}continue;}');f.write_text(t)
f=p/'tests/integration.cpp';t=f.read_text().replace('movie,-1,-1,0,url','movie,-1,-1,"",url');f.write_text(t)
f=p/'tools/build.sh';t=f.read_text().replace(' DOTNET_ROLL_FORWARD=LatestMajor', ' export DOTNET_ROLL_FORWARD=LatestMajor\n export WSLENV="${WSLENV:+$WSLENV:}DOTNET_ROLL_FORWARD"\n ');f.write_text(t,newline='\n')
