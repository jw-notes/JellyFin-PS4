from pathlib import Path
import urllib.request,tarfile
root=Path('tools/ps4-portlibs-107');sources=root/'sources';sources.mkdir(exist_ok=True)
for name,url in [('zlib-1.3.1.tar.gz','https://zlib.net/fossils/zlib-1.3.1.tar.gz'),('bzip2-1.0.8.tar.gz','https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz')]:
 dest=sources/name
 if not dest.exists():urllib.request.urlretrieve(url,dest)
 with tarfile.open(dest) as tf:tf.extractall(sources,filter='data')
print('Downloaded compression dependency source locally')
