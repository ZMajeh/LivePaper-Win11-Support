// Measures the heap usage of the video decoding loop, to detect memory leaks.
// The heap is reported after every loop of the video.
//
// Build:
//   gcc -O0 -g -o leaktest leaktest.c src/video.c src/log.c src/fail.c -I src \
//       $(pkg-config --cflags --libs sdl2 libavformat libavcodec libavutil libswscale) -lm
// Run:
//   SDL_VIDEODRIVER=dummy ./leaktest assets/test.gif 8
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <SDL.h>
#include "video.h"
#include "platform.h"

// The number of bytes that are currently allocated on the heap.
static long HeapInUse(void) {
    struct mallinfo2 mi = mallinfo2();
    return (long)mi.uordblks;
}

int main(int argc, char **argv) {
    const char *file = argc > 1 ? argv[1] : "assets/test.gif";
    int loops = argc > 2 ? atoi(argv[2]) : 6;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *w = SDL_CreateWindow("leaktest", 0, 0, 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);

    struct Context ctx = {0};
    ctx.window = w;
    ctx.renderer = r;
    ctx.file = strdup(file);
    ctx.fit = FIT_FIT;

    struct Video *v = VideoLoad(&ctx);
    long base = 0;
    for (int i = 0; i < loops; i++) {
        for (int f = 0; f < 400; f++)   // roughly one loop of the video
            VideoUpdate(0.01, v, &ctx);
        long h = HeapInUse();
        if (i == 0) base = h;
        printf("loop %2d: heap in use = %8.2f MiB   (delta from loop 1: %+8.2f MiB)\n",
               i, h / 1048576.0, (h - base) / 1048576.0);
        fflush(stdout);
    }
    VideoClear(v, &ctx);
    return 0;
}