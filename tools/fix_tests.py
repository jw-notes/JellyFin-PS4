from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'tests/integration.cpp';t=f.read_text().replace('c\\\\n\\\"','c\\\\u000a\\\"');f.write_text(t)
f=p/'tools/setup_test_server.py';t=f.read_text().replace(" req('/Startup/User',", " req('/Startup/User')\n req('/Startup/User',");f.write_text(t)
