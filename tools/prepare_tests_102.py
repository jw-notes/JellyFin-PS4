from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/keyboard_model.cpp';t=f.read_text().replace('if(old){input.replace(0,old,key.value);','if(old){if(input.size()-old+key.value.size()>250)break;input.replace(0,old,key.value);');f.write_text(t,newline='\n')
f=p/'tests/auth_integration.cpp';t=f.read_text().replace('    if (argc < 4) return 0;', '''    if(argc==3 && std::string(argv[1])=="--probe"){
        assert(storage_init(44));
        assert(connect_server(argv[2]));
        printf("PASS complete server URL probe: %s\\n",g_server);
        return 0;
    }
    if (argc < 4) return 0;''').replace('assert(connect_server(endpoint.address, endpoint.port));','assert(connect_server(argv[1]));');f.write_text(t,newline='\n')
