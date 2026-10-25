// BMP encoder adapted from the original PS3 util/screenshot.cpp.
#include "screenshot.h"
#include "app.h"
#include "platform.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>
static void put16(FILE*f,unsigned v){fputc(v&255,f);fputc((v>>8)&255,f);}
static void put32(FILE*f,unsigned v){put16(f,v&65535);put16(f,v>>16);}
bool write_frame_bmp(FILE*f,const uint32_t*frame,unsigned width,unsigned height,unsigned pitch){
 if(!f||!frame||!width||!height||width>4096||height>4096||pitch<width)return false;
 const unsigned row=(width*3+3)&~3u,size=54+row*height;
 fputc('B',f);fputc('M',f);put32(f,size);put32(f,0);put32(f,54);put32(f,40);put32(f,width);put32(f,height);put16(f,1);put16(f,24);put32(f,0);put32(f,row*height);put32(f,2835);put32(f,2835);put32(f,0);put32(f,0);
 std::vector<unsigned char> output(row);
 for(int y=(int)height-1;y>=0;--y){for(unsigned x=0;x<width;x++){auto pixel=frame[y*pitch+x];output[x*3]=(unsigned char)pixel;output[x*3+1]=(unsigned char)(pixel>>8);output[x*3+2]=(unsigned char)(pixel>>16);}if(fwrite(output.data(),1,row,f)!=row)return false;}
 return !ferror(f);
}
void capture_screen(const uint32_t* frame){
 const char* directory="/data/JellyFin-PS4-logs/screenshots";
 if(mkdir(directory,0755)==0)chmod(directory,0755);
 static unsigned sequence=0;
 char path[256];int fd=-1;
 for(int attempt=0;attempt<100&&fd<0;attempt++){snprintf(path,sizeof path,"%s/screen-%lld-%u.bmp",directory,(long long)time(nullptr),++sequence);fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0644);}
 if(fd<0){plog("screenshot: unable to create capture file");return;}
 fchmod(fd,0644);
 FILE* file=fdopen(fd,"wb");if(!file){close(fd);plog("screenshot: unable to open capture stream");return;}
 bool ok=write_frame_bmp(file,frame,WIDTH,HEIGHT,WIDTH);if(fclose(file))ok=false;
 plog(ok?(std::string("screenshot: saved ")+path).c_str():"screenshot: write failed");
}
