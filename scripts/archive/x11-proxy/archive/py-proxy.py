#!/usr/bin/env python3
import socket, threading, os, sys

VSOCK_PORT = 6000
UNIX_PATH = "/tmp/.X11-unix/X1"

def handle(client):
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.connect(UNIX_PATH)
    def forward(src, dst):
        try:
            while True:
                data = src.recv(4096)
                if not data: break
                dst.sendall(data)
        finally:
            try: src.close()
            except: pass
            try: dst.close()
            except: pass

    t1 = threading.Thread(target=forward, args=(client, srv), daemon=True)
    t2 = threading.Thread(target=forward, args=(srv, client), daemon=True)
    t1.start(); t2.start()
    t1.join(); t2.join()

vs = socket.socket(socket.AF_VSOCK, socket.SOCK_STREAM)
vs.bind((socket.VMADDR_CID_ANY, VSOCK_PORT))
vs.listen(50)
print("listening")
while True:
    c, addr = vs.accept()
    threading.Thread(target=handle, args=(c,), daemon=True).start()

