#include "app.h"
#include "crash.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
std::string data_dir="build/crash-test-108";void plog(const char*){}
int main(){mkdir(data_dir.c_str(),0700);pid_t child=fork();assert(child>=0);if(!child){rlimit limit{0,0};setrlimit(RLIMIT_CORE,&limit);install_crash_diagnostics(108);raise(SIGABRT);_exit(1);}int status;waitpid(child,&status,0);assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT);std::ifstream input(data_dir+"/crash.log");std::string log((std::istreambuf_iterator<char>(input)),{});assert(log.find("fatal signal=0x06")!=std::string::npos);puts("PASS fatal signal log preserves the original fatal signal, without creating a test core dump");}
