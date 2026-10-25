from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/http_ps4.cpp';t=f.read_text().replace('int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){','static int perform(int method,const char* url,const char* body,const char* token,char* out,int cap,int* count){\n if(count)*count=0;');t=t.replace('out[used]=0;if(n<0)', 'out[used]=0;if(count)*count=used;if(n<0)');t+='''
int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){return perform(method,url,body,token,out,cap,nullptr);}
int http_fetch_binary(const char* url,const char* token,uint8_t* out,int cap){int count=0;int st=perform(HTTP_GET,url,nullptr,token,(char*)out,cap,&count);return st==200?count:-1;}
''';f.write_text(t)
f=p/'source/app.cpp';t=f.read_text().replace('a++;int n=0;std::string obj;', 'a++;std::string obj;').replace('n++; std::string candidate','std::string candidate');f.write_text(t)
