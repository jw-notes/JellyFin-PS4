from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/main.cpp';t=f.read_text().replace('135+i*67','170+i*67').replace('147+i*67','182+i*67');f.write_text(t,newline='\n')
f=p/'source/login_ui.cpp';t=f.read_text().replace('static void login_header(const std::string& title){app_header(title,"");}', 'static void login_header(const std::string& title){app_header("","");text(400,28,title,g_theme.text,24,800);}');f.write_text(t,newline='\n')
f=p/'tests/http_host.cpp';t=f.read_text().replace('int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){', 'static int perform_host(int method,const char* url,const char* body,const char* token,char* out,int cap,int* count){\n if(count)*count=0;');t=t.replace('memcpy(out,result.data(),pos);out[pos]=0;','memcpy(out,result.data(),pos);out[pos]=0;if(count)*count=(int)pos;');t+='''
int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){return perform_host(method,url,body,token,out,cap,nullptr);}
int http_fetch_binary(const char* url,const char* token,uint8_t* out,int cap){int count=0;int status=perform_host(HTTP_GET,url,"",token,(char*)out,cap,&count);return status==200?count:-1;}
''';f.write_text(t,newline='\n')
