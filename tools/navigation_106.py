from pathlib import Path
f=Path('source/main.cpp');t=f.read_text()
a=t.index('static void settings_screen()');b=t.index('static void play(',a)
s=t[a:b].replace('static void settings_screen()','static int settings_screen()')
s=s.replace('auto b=press();','auto b=press();if(b&ORBIS_PAD_BUTTON_L1)return 1;if(b&ORBIS_PAD_BUTTON_R1)return 0;')
s=s.replace('logout();return;','logout();return -1;').replace('running=false;return;','running=false;return -1;').replace('if(b&ORBIS_PAD_BUTTON_CIRCLE)return;','if(b&ORBIS_PAD_BUTTON_CIRCLE)return -1;')
s=s.replace('   Circle: return','   L1: Search   R1: Home   Circle: return')
t=t[:a]+s+'''static int search_screen(std::string& query){
 UiInputScope input_scope;
 for(;;){draw_search(query);present();auto b=press();
  if(b&ORBIS_PAD_BUTTON_L1)return 0;
  if(b&ORBIS_PAD_BUTTON_R1)return 2;
  if(b&ORBIS_PAD_BUTTON_CIRCLE)return -1;
  if(b&ORBIS_PAD_BUTTON_CROSS){if(keyboard("Search Jellyfin",query)&&!query.empty())return 3;ui_input_sync();}
  usleep(16000);
 }
}
'''+t[b:]
t=t.replace('int header_focus=-1;', 'int header_focus=-1,pending_tab=-1;')
needle='auto& loc=stack.back();'
insert='''if(pending_tab>=0){
  int tab=pending_tab;pending_tab=-1;plog((std::string("Navigation: tab=")+(tab==0?"Home":tab==1?"Search":"Settings")).c_str());
  if(tab==0){stack.resize(1);header_focus=-1;reload=true;}
  else if(tab==1){std::string query;int next=search_screen(query);if(next==3){stack.push_back({"","Search: "+query,query,0,0,0});reload=true;header_focus=-1;}else if(next>=0)pending_tab=next;}
  else{int next=settings_screen();if(next>=0)pending_tab=next;header_focus=-1;reload=true;}
  ui_input_sync();continue;
 }
 '''
t=t.replace(needle,insert+needle)
a=t.index(' if((b&ORBIS_PAD_BUTTON_OPTIONS)');b=t.index(' if((b&ORBIS_PAD_BUTTON_TRIANGLE)',a)
t=t[:a]+''' if(b&(ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1)){int current=loc.query.empty()?0:1;pending_tab=(current+((b&ORBIS_PAD_BUTTON_L1)?2:1))%3;header_focus=-1;ui_input_sync();continue;}
 if((b&ORBIS_PAD_BUTTON_OPTIONS)||(header_focus==2&&(b&ORBIS_PAD_BUTTON_CROSS))){pending_tab=2;ui_input_sync();continue;}
 if(header_focus==1&&(b&ORBIS_PAD_BUTTON_CROSS)){pending_tab=1;ui_input_sync();continue;}
'''+t[b:]
t=t.replace('else if(b&ORBIS_PAD_BUTTON_LEFT)loc.selected=std::max(0,loc.selected-1);','else if(b&ORBIS_PAD_BUTTON_LEFT){if(loc.selected==0&&loc.start>=60){loc.start-=60;loc.selected=59;reload=true;}else loc.selected=std::max(0,loc.selected-1);}')
t=t.replace('else if(b&ORBIS_PAD_BUTTON_RIGHT)loc.selected=std::min(std::max(0,count-1),loc.selected+1);','else if(b&ORBIS_PAD_BUTTON_RIGHT){if(loc.selected==count-1&&loc.start+60<page.total){loc.start+=60;loc.selected=0;reload=true;}else loc.selected=std::min(std::max(0,count-1),loc.selected+1);}')
t=t.replace(' else if((b&ORBIS_PAD_BUTTON_L1)&&loc.start>=60){loc.start-=60;loc.selected=0;reload=true;}\n','').replace(' else if((b&ORBIS_PAD_BUTTON_R1)&&loc.start+60<page.total){loc.start+=60;loc.selected=0;reload=true;}\n','')
f.write_text(t,newline='\n')
