#!/bin/bash -e

echo "VSOCK-LISTENER activated --> Sending to /tmp/.x11-unix/x1"
# socat VSOCK-LISTEN:6000,fork UNIX-CONNECT:/tmp/.X11-unix/X1
# sudo socat -d -d VSOCK-LISTEN:6000,fork,reuseaddr,max-children=50 UNIX-CONNECT:/tmp/.X11-unix/X1
# Source's startup burst exceeds Linux's 256 KiB default VSOCK stream buffer. Increase both the
# per-socket maximum and active size to 16 MiB before listen(2), so accepted sockets inherit it.
# The binary socket-option values below are little-endian uint64_t values; level 40 is AF_VSOCK.
sudo socat -b 1048576 -d -d 'VSOCK-LISTEN:6000,fork,reuseaddr,max-children=50,linger=5,sockopt-sock=40:2:x0000000100000000,sockopt-sock=40:0:x0000000100000000' UNIX-CONNECT:/tmp/.X11-unix/X0
# sudo socat -d -d TCP-LISTEN:6000,fork,reuseaddr,max-children=50,linger=5 UNIX-CONNECT:/tmp/.X11-unix/X0
#sudo socat VSOCK-LISTEN:6000,fork,reuseaddr,max-children=50 UNIX-CONNECT:/tmp/.X11-unix/X1
# socat VSOCK-LISTEN:6000,reuseaddr,fork UNIX-CONNECT:/tmp/.X11-unix/X1

# socat -d -d -v VSOCK-LISTEN:6000,reuseaddr,fork UNIX-CONNECT:/tmp/.X11-unix/X1
# 2>&1 | tee /tmp/socat.log
