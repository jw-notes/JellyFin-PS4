from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/app.cpp';t=f.read_text()
needle='\\\"SubtitleProfiles\\\":['
codec='\\\"CodecProfiles\\\":[{\\\"Type\\\":\\\"Video\\\",\\\"Codec\\\":\\\"h264\\\",\\\"Conditions\\\":[{\\\"Condition\\\":\\\"LessThanEqual\\\",\\\"Property\\\":\\\"VideoBitDepth\\\",\\\"Value\\\":\\\"8\\\",\\\"IsRequired\\\":true},{\\\"Condition\\\":\\\"LessThanEqual\\\",\\\"Property\\\":\\\"Width\\\",\\\"Value\\\":\\\"1920\\\",\\\"IsRequired\\\":true},{\\\"Condition\\\":\\\"LessThanEqual\\\",\\\"Property\\\":\\\"Height\\\",\\\"Value\\\":\\\"1080\\\",\\\"IsRequired\\\":true}]},{\\\"Type\\\":\\\"VideoAudio\\\",\\\"Codec\\\":\\\"aac\\\",\\\"Conditions\\\":[{\\\"Condition\\\":\\\"Equals\\\",\\\"Property\\\":\\\"AudioSampleRate\\\",\\\"Value\\\":\\\"48000\\\",\\\"IsRequired\\\":true}]}],'
assert needle in t;t=t.replace(needle,codec+needle);f.write_text(t)
f=p/'source/main.cpp';t=f.read_text().replace('uint64_t base=settings.resume?item.resume:0;', 'uint64_t base=settings.resume&&!settings.direct?item.resume:0;bool direct_resume_pending=settings.direct&&settings.resume&&item.resume;')
t=t.replace('if(got||player_active())started=true;', 'if(got||player_active())started=true;if(started&&direct_resume_pending){player_seek(item.resume);direct_resume_pending=false;}');f.write_text(t)
