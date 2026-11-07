// Media on USB drives: see local_media.h.
#include "local_media.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <ctype.h>

namespace {
std::string usb_root(){
#ifdef HOST_TEST
 const char* root=getenv("JF_USB_ROOT");return root?root:"/tmp/jf-usb";
#else
 return "/mnt";
#endif
}
std::string lower_ext(const std::string& name){const auto dot=name.rfind('.');if(dot==std::string::npos)return {};std::string e=name.substr(dot+1);for(char& c:e)c=(char)tolower((unsigned char)c);return e;}
LocalKind kind_of(const std::string& name){
 static const char* video[]={"mkv","mp4","m4v","mov","avi","ts","m2ts","mts","webm","wmv","mpg","mpeg","flv","3gp"};
 static const char* audio[]={"mp3","flac","m4a","aac","ogg","oga","opus","wav","wma","alac","aiff","aif"};
 const std::string e=lower_ext(name);
 for(auto v:video)if(e==v)return LOCAL_VIDEO;
 for(auto a:audio)if(e==a)return LOCAL_AUDIO;
 return LOCAL_OTHER;
}
// Natural order: "Track 2" before "Track 10".
bool natural_less(const std::string& a,const std::string& b){
 size_t i=0,j=0;
 while(i<a.size()&&j<b.size()){
  if(isdigit((unsigned char)a[i])&&isdigit((unsigned char)b[j])){
   size_t i2=i,j2=j;while(i2<a.size()&&isdigit((unsigned char)a[i2]))++i2;while(j2<b.size()&&isdigit((unsigned char)b[j2]))++j2;
   const std::string na=a.substr(i,i2-i),nb=b.substr(j,j2-j);const auto sa=na.find_first_not_of('0'),sb=nb.find_first_not_of('0');
   const std::string ta=sa==std::string::npos?"":na.substr(sa),tb=sb==std::string::npos?"":nb.substr(sb);
   if(ta.size()!=tb.size())return ta.size()<tb.size();if(ta!=tb)return ta<tb;i=i2;j=j2;continue;}
  const int ca=tolower((unsigned char)a[i]),cb=tolower((unsigned char)b[j]);if(ca!=cb)return ca<cb;++i;++j;}
 return a.size()-i<b.size()-j;
}
}  // namespace

std::vector<std::string> usb_drives(){
 std::vector<std::string> out;
 for(int i=0;i<8;++i){const std::string slot=usb_root()+"/usb"+std::to_string(i);DIR* d=opendir(slot.c_str());if(!d)continue;
  bool any=false;while(auto e=readdir(d))if(strcmp(e->d_name,".")&&strcmp(e->d_name,"..")){any=true;break;}closedir(d);if(any)out.push_back(slot);}
 return out;
}
// On the console stat() cannot be trusted: the toolchain's struct stat does
// not match the kernel's, so its sizes are wrong (65984 for a 33 MB file).
// A folder is one that opens; a size is found by seeking to the end.
bool local_is_dir(const std::string& path){
#ifdef HOST_TEST
 struct stat sb;return stat(path.c_str(),&sb)==0&&S_ISDIR(sb.st_mode);
#else
 DIR* d=opendir(path.c_str());if(!d)return false;closedir(d);return true;
#endif
}
uint64_t local_file_size(const std::string& path){
#ifdef HOST_TEST
 struct stat sb;return stat(path.c_str(),&sb)==0?(uint64_t)sb.st_size:0;
#else
 FILE* f=fopen(path.c_str(),"rb");if(!f)return 0;
 const off_t size=fseeko(f,0,SEEK_END)==0?ftello(f):0;fclose(f);return size>0?(uint64_t)size:0;
#endif
}
std::string usb_drives_signature(){std::string s;for(const auto& d:usb_drives())s+=d+";";return s;}
bool list_local_folder(const std::string& path,std::vector<LocalEntry>& out){
 out.clear();DIR* d=opendir(path.c_str());if(!d)return false;
 while(auto e=readdir(d)){
  const std::string name=e->d_name;if(name.empty()||name[0]=='.'||name=="System Volume Information"||name=="$RECYCLE.BIN")continue;
  LocalEntry entry;entry.name=name;entry.path=path+"/"+name;
  if(local_is_dir(entry.path))entry.kind=LOCAL_FOLDER;
  else{entry.kind=kind_of(name);if(entry.kind!=LOCAL_OTHER)entry.bytes=local_file_size(entry.path);
   // A download's description, and a file still downloading, are not media.
   if(entry.kind==LOCAL_OTHER)continue;}
  out.push_back(entry);}
 closedir(d);
 std::sort(out.begin(),out.end(),[](const LocalEntry& a,const LocalEntry& b){if((a.kind==LOCAL_FOLDER)!=(b.kind==LOCAL_FOLDER))return a.kind==LOCAL_FOLDER;return natural_less(a.name,b.name);});
 return true;
}
Item local_item(const LocalEntry& entry){
 Item item;
 // A stable id from the path, so nothing asks the server about it by mistake.
 unsigned long h=5381;for(unsigned char c:entry.path)h=h*33+c;snprintf(item.base.id,sizeof item.base.id,"local:%08lx",h&0xffffffffUL);
 std::string name=entry.name;const auto dot=name.rfind('.');if(dot!=std::string::npos&&dot>0)name.resize(dot);
 snprintf(item.base.name,sizeof item.base.name,"%s",name.c_str());
 snprintf(item.base.type,sizeof item.base.type,"%s",entry.kind==LOCAL_AUDIO?"Audio":"Video");
 // Music: the folder is the album.
 if(entry.kind==LOCAL_AUDIO){const auto slash=entry.path.rfind('/');if(slash!=std::string::npos){const std::string dir=entry.path.substr(0,slash);const auto s2=dir.rfind('/');item.album=s2==std::string::npos?dir:dir.substr(s2+1);}}
 item.local=entry.path;return item;
}
