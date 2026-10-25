from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4'); v=p/'vendor/ps3/source'; s=p/'source'
(s/'compat/ppu-types.h').write_text('#pragma once\n#include <stdint.h>\ntypedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int32_t s32;\n')
t=(v/'ui/theme.cpp').read_text(); t=t.replace('#include <sys/file.h>', '#include <dirent.h> // POSIX PS4 directory enumeration\n//')
t=t.replace('/dev_hdd0/game/JFPS30000/USRDIR/themes','/app0/themes').replace('/dev_hdd0/tmp/themes','/data/JellyFin-PS4/themes')
t=t.replace('s32 fd = -1;\n        if (sysLv2FsOpenDir(THEME_DIRS[d], &fd) != 0) continue;', 'DIR *fd = opendir(THEME_DIRS[d]);\n        if (!fd) continue;')
t=t.replace('sysFSDirent ent;\n        u64 read = 0;\n        while (n < max && sysLv2FsReadDir(fd, &ent, &read) == 0 && read != 0) {','struct dirent *entry;\n        while (n < max && (entry = readdir(fd)) != NULL) {\n            const struct dirent &ent = *entry;')
t=t.replace('sysLv2FsCloseDir(fd);','closedir(fd);'); (s/'theme.cpp').write_text(t)
t=(v/'api/jellyfin_api.cpp').read_text(); t=t.replace('#include "json_unescape.h"','#include "json_lite.h"\n#include "json_unescape.h"')
a=t.index('    char search[64];',t.index('int json_get_string')); b=t.index('\n}',a)
t=t[:a]+'''    if (!json || !out || out_size <= 0) return 0;
    const char *end = json + strlen(json);
    const char *value = find_top_value(json, end, key);
    out[0] = 0;
    return value && read_json_string(value, end, out, out_size);'''+t[b:]
a=t.index('    char search[64];',t.index('int json_get_in_range')); b=t.index('\n}',a)
t=t[:a]+'''    if (!start || !out || out_size <= 0) return 0;
    const char *value = find_top_value(start, start + len, key);
    out[0] = 0;
    return value && read_json_string(value, start + len, out, out_size);'''+t[b:]
(s/'core.cpp').write_text(t)
(p/'LICENSE').write_bytes((p/'vendor/ps3/LICENSE').read_bytes())
