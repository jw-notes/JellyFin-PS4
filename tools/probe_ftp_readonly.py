import socket
try:
 s=socket.create_connection(('192.168.1.26',2121),5)
 print(s.recv(256).decode(errors='replace'))
 s.close()
except OSError as e: print(type(e).__name__+': '+str(e))
