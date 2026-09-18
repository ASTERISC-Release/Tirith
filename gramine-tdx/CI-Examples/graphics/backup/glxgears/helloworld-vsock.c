/* x11v.c  – X11 “hello world” over vsock, public domain */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <stdint.h>
#include <X11/Xproto.h>          /* xcb/proto headers are nicer, but we stay pure */
#include <stdlib.h>

static int vsock_connect(uint32_t cid, uint32_t port)
{
    int s = socket(AF_VSOCK, SOCK_STREAM, 0);
    struct sockaddr_vm addr = {
        .svm_family = AF_VSOCK,
        .svm_cid    = cid,
        .svm_port   = port,
    };
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("vsock connect"); return -1;
    }
    return s;
}

/* write exact sz bytes */
static void write_exact(int fd, const void *buf, size_t sz)
{ size_t off = 0; while (off < sz) off += write(fd, (char *)buf + off, sz - off); }

/* read exact sz bytes */
static void read_exact (int fd, void *buf, size_t sz)
{ size_t off = 0; while (off < sz) off += read (fd, (char *)buf + off, sz - off); }

static uint32_t mk_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{ return (a<<24)|(b<<16)|(c<<8)|d; }

int main(void)
{
    int xfd = vsock_connect(0, 6000);          /* host CID 0, X port 6000 */
    if (xfd < 0) return 1;

    printf("Connected to X11 server over vsock\n");

    /* ---------- 1.  X11 connection setup ---------- */
    struct {
        uint8_t  byte_order;
        uint8_t  pad;
        uint16_t proto_major, proto_minor;
        uint16_t auth_proto_len, auth_data_len;
        uint16_t pad2;
    } __attribute__((packed)) hello = {
        .byte_order     = 'l',                 /* little endian */
        .pad            = 0,
        .proto_major    = 11,
        .proto_minor    = 0,
        .auth_proto_len = 0,                  /* no auth (MIT-MAGIC-COOKIE-1 would go here) */
        .auth_data_len  = 0,
        .pad2           = 0,
    };
    write_exact(xfd, &hello, sizeof(hello));

    /* ---------- 2.  read server reply ---------- */
    uint32_t rlen;  read_exact(xfd, &rlen, 4);  rlen <<= 2; /* length in 4-byte units */
    unsigned char *reply = malloc(rlen);
    read_exact(xfd, reply, rlen);

    uint32_t rid  = ((uint32_t *)reply)[4];      /* resource-id base */
    uint32_t mask = ((uint32_t *)reply)[7];      /* resource-id mask */
    free(reply);

    /* convenient ID allocators */
    #define ID (rid++)
    uint32_t root        = ((uint32_t *)reply)[5];
    uint32_t white_pixel = ((uint32_t *)reply)[20];
    /* we ignore depths/visuals – white_pixel is already given */

    /* ---------- 3.  open font, create GC, create window ---------- */
    uint32_t font  = ID;
    uint32_t gc    = ID;
    uint32_t win   = ID;

    /* OpenFont */
    char fontname[] = "fixed";
    uint32_t opf[] = {
        mk_uint32(45,0,0,3+strlen(fontname)/4),  /* request header */
        font,
        strlen(fontname),
    };
    write_exact(xfd, opf, sizeof(opf));
    write_exact(xfd, fontname, strlen(fontname));

    /* CreateGC */
    uint32_t cgc[] = {
        mk_uint32(55,0,0,4),
        gc, root,
        1<<0 | 1<<1 | 1<<3,                 /* valuemask: foreground|background|font */
        white_pixel, 0, font,
    };
    write_exact(xfd, cgc, sizeof(cgc));

    /* CreateWindow */
    uint32_t cw[] = {
        mk_uint32(1,0,0,8),
        win, root,
        50,50, 200,120, 0,
        1<<0,                               /* class = CopyFromParent */
        0,0,
        1<<11, 0                            /* event mask = Exposure */
    };
    write_exact(xfd, cw, sizeof(cw));

    /* MapWindow */
    uint32_t mw = mk_uint32(8,0,0,2);
    write_exact(xfd, &mw, 4);
    write_exact(xfd, &win, 4);

    /* ---------- 4.  event loop ---------- */
    // while (1) {
    //     uint8_t ev[32];
    //     read_exact(xfd, ev, 32);
    //     if (ev[0] == KeyPress && ev[1] == 16) break;   /* q key */
    // }
    close(xfd);
    return 0;
}
