from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'tools/build.sh';t=f.read_text().replace('-fPIC -funwind-tables','-D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -fPIC -funwind-tables');f.write_text(t)
f=p/'source/app.cpp';t=f.read_text().replace('static thread_local std::string p','static std::string p');t=t.replace('FILE* f=fopen(jf_data_path("app.log"),"a")','FILE* f=fopen((data_dir+"/app.log").c_str(),"a")');f.write_text(t)
f=p/'tools/package.py';t=f.read_text().replace("for file in (p/'vendor/ps3/themes').glob('*.ini'):shutil.copyfile(file,pkg/'themes'/file.name)","for file in (p/'vendor/ps3/themes').rglob('*.ini'):shutil.copyfile(file,pkg/'themes'/file.name)");f.write_text(t)
