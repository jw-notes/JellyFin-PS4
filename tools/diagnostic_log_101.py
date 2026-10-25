from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/app.cpp';t=f.read_text().replace('std::string device_id, data_dir, last_error;', 'std::string device_id, data_dir, last_error;\nstatic std::string diagnostic_log_path;')
a=t.index('void plog(');b=t.index('\nvoid crash_log',a)
t=t[:a]+'''static void append_log(const std::string& path,const char* message){
    FILE* file=fopen(path.c_str(),"a");
    if(file){fprintf(file,"%lld %s\\n",(long long)time(nullptr),message);fclose(file);}
}
void plog(const char* message){
    if(data_dir.empty())return;
    append_log(data_dir+"/app.log",message);
    // Only diagnostic messages reach this file: no passwords, tokens or
    // Quick Connect secrets are logged. The private session folder stays 0700.
    if(!diagnostic_log_path.empty())append_log(diagnostic_log_path,message);
}'''+t[b:]
t=t.replace('data_dir=dir;mkdir(data_dir.c_str(),0700);', '''data_dir=dir;mkdir(data_dir.c_str(),0700);
 mkdir("/data/JellyFin-PS4-logs",0755);
 char log_path[160];snprintf(log_path,sizeof log_path,"/data/JellyFin-PS4-logs/user-%08x.log",user);
 diagnostic_log_path=log_path;''')
f.write_text(t,newline='\n')
