/* vsock-xproxy.c  – public domain, ~60 lines */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/vm_sockets.h>

static void copy(int src, int dst)          /* trivial bidirectional forwarder */
{
    char buf[64*1024];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0)
        if (write(dst, buf, n) != n) break;
}

int main(void)
{
    int vsock = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (vsock < 0) { perror("socket(AF_VSOCK)"); return 1; }

    struct sockaddr_vm addr = {
        .svm_family = AF_VSOCK,
        .svm_cid    = VMADDR_CID_ANY,   /* 0xffffffff = “any CID” – host */
        .svm_port   = 6000,
    };
    if (bind(vsock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(vsock, 5) < 0) { perror("listen"); return 1; }

    printf("vsock-xproxy: listening on CID 0, port 6000 – forwarding to /tmp/.X11-unix/X1_\n");

    while (1) {
        struct sockaddr_vm guest;
        socklen_t len = sizeof(guest);
        int s = accept(vsock, (struct sockaddr *)&guest, &len);
        if (s < 0) { perror("accept"); continue; }

        /* open the real X Unix socket */
        int x11 = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un un = { .sun_family = AF_UNIX,
                                  .sun_path   = "/tmp/.X11-unix/X1" };
        if (connect(x11, (struct sockaddr *)&un, sizeof(un)) < 0) {
            perror("connect(/tmp/.X11-unix/X0)");
            close(s); close(x11);
            continue;
        }

        printf("new guest CID %u  ->  X11\n", guest.svm_cid);

        if (fork() == 0) {              /* child handles one connection */
            close(vsock);
            /* two unidirectional copies */
            if (fork() == 0) { copy(s, x11); _exit(0); }
            copy(x11, s);
            _exit(0);
        }
        close(s); close(x11);           /* parent loops */
    }
}
