/* openx.c  –  compile:  gcc openx.c -lX11  */
#include <X11/Xlib.h>
//#include <client/glimpl.h>
#include <stdio.h>
#include <stdlib.h>

void test_xauth(void)
{
    FILE *file = fopen("/home/adil/.Xauthority", "r");
    if (file) {
        printf("Xauthority file found.\n");
        
        int c;
        while ((c = fgetc(file)) != EOF) {
          putchar(c);
        }

      if (ferror(file)) {
        perror("fgetc");
        fclose(file);
      }
      
      fclose(file);
    } else {
        perror("Xauthority file not found");
    }
}

int main(void)
{
    //test_xauth();

    /* Set Display environment variable */
    fprintf(stderr, "Setting DISPLAY environment variable to 2:0\n");
    if (setenv("DISPLAY", "1:0", 1) == 0) {
      fprintf(stderr, "Environment variable DISPLAY set successfully.\n");
    } else {
      perror("setenv failed");
    }

    /* Enter and check if sharedgl is working */
    // sharedgl_entry();

    Display *d = XOpenDisplay(NULL);
    if (!d) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }

    int s              = DefaultScreen(d);
    Window w           = XCreateSimpleWindow(
                             d,
                             RootWindow(d, s),
                             10, 10,               /* x, y */
                             400, 300,             /* width, height */
                             0,                    /* border width */
                             BlackPixel(d, s),     /* border colour */
                             WhitePixel(d, s));    /* background colour */

    XSelectInput(d, w, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(d, w);

    /* event loop – waits until window is closed */
    for (XEvent e; ; XNextEvent(d, &e))
        if (e.type == ClientMessage || e.type == DestroyNotify)
            break;

    XCloseDisplay(d);
    return 0;
}
