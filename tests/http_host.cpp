// Host-only test transport. Runs the real curl client without a shell.
#include "app.h"
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <vector>
int http_init(){return 0;}void http_end(){}
static int perform_host(int method,const char* url,const char* body,const char* token,char* out,int cap,int* count){
 if(count)*count=0;
 int in[2],output[2];if(pipe(in)||pipe(output))return -1;
 std::string auth="Authorization: MediaBrowser Client=\"JellyFin PS4\", Device=\"PS4\", DeviceId=\""+device_id+"\", Version=\"0.1.0\"";
 std::string tok="X-Emby-Token: "+std::string(token?token:"");
 std::vector<std::string> args={"curl","--silent","--show-error","--max-time","35","--request",method==HTTP_POST?"POST":method==HTTP_DELETE?"DELETE":"GET","--header",auth,"--header",tok,"--header","Content-Type: application/json","--write-out","\n%{http_code}",url};
 if(body&&*body){args.push_back("--data-binary");args.push_back("@-");}
 pid_t pid=fork();if(pid==0){dup2(in[0],STDIN_FILENO);dup2(output[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(output[0]);close(output[1]);std::vector<char*> argv;for(auto&s:args)argv.push_back(const_cast<char*>(s.c_str()));argv.push_back(nullptr);execvp("curl",argv.data());_exit(127);}
 close(in[0]);close(output[1]);if(body&&*body){size_t at=0,n=strlen(body);while(at<n){ssize_t z=write(in[1],body+at,n-at);if(z<=0)break;at+=z;}}close(in[1]);std::string result;char b[8192];ssize_t n;while((n=read(output[0],b,sizeof b))>0)result.append(b,n);close(output[0]);int code;waitpid(pid,&code,0);out[0]=0;if(!WIFEXITED(code)||WEXITSTATUS(code))return -1;auto pos=result.rfind('\n');if(pos==std::string::npos)return -1;int status=atoi(result.c_str()+pos+1);if(pos>=(size_t)cap)return -1;memcpy(out,result.data(),pos);out[pos]=0;if(count)*count=(int)pos;if(status==401&&token&&*token)g_auth_expired=true;return status;
}
std::string http_failure_detail(){return "The host HTTP client could not reach the server.";}

int http_request(int method,const char* url,const char* body,const char* token,char* out,int cap){return perform_host(method,url,body,token,out,cap,nullptr);}
int http_fetch_binary(const char* url,const char* token,uint8_t* out,int cap){int count=0;int status=perform_host(HTTP_GET,url,"",token,(char*)out,cap,&count);return status==200?count:-1;}
