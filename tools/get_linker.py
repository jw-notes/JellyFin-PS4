import pathlib, urllib.request, re
root=pathlib.Path('/mnt/d/Coding-Projects/PS4/JellyFin-PS4/tools')
u='https://deb.debian.org/debian/pool/main/l/llvm-toolchain-18/'
s=urllib.request.urlopen(u).read().decode()
n=re.findall(r'href="(lld-18_[^"]*amd64.deb)"',s)[-1]
print(n,flush=True)
(root/'lld.deb').write_bytes(urllib.request.urlopen(u+n).read())
