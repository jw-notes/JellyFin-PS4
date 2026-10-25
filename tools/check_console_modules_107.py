from ftplib import FTP
f=FTP();f.connect('192.168.1.26',2121,timeout=10);f.login();f.voidcmd('TYPE I')
for path in ['/system/common/lib/libScePigletv2VSH.sprx','/system/common/lib/libSceLibcInternal.sprx','/system/common/lib/libSceVideodec2.sprx']:
 try:print(path,'bytes=',f.size(path))
 except Exception as e:print(path,type(e).__name__,str(e))
f.quit()
