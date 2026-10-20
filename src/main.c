#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <argtable3.h>

#include "fail.h"
#include "log.h"
#include "platform.h"
#include "sysinfo.h"
#include "video.h"

Uint32 g_open_file_event = 0;

// Set when the user asked for the periodic statistics output.
static bool g_print_stats = false;

static void LogMemoryUsage(const char *prefix) {
    struct MemoryInfo mem;
    if (SysGetMemoryInfo(&mem) != 0) {
        LOG_DEBUG("%smemory usage is not available on this platform", prefix);
        return;
    }
    char rss[32], peak[32], total[32];
    SysFormatBytes(mem.process_rss, rss, sizeof(rss));
    SysFormatBytes(mem.process_peak_rss, peak, sizeof(peak));
    SysFormatBytes(mem.system_total, total, sizeof(total));
    LOG_INFO("%smemory: rss=%s peak_rss=%s system_total=%s", prefix, rss, peak, total);
}

// Draws a colour over the whole window and checks whether it really shows up
// on the screen. This finds out whether the wallpaper window is composited by
// the system at all.
// The check is repeated, because the system needs a moment to compose a new
// frame and the result is not always the same.
static bool RendererIsOnScreen(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);   // magenta
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    for (int attempt = 0; attempt < 5; attempt++) {
        SDL_Delay(100);   // give the system a moment to compose the frame
        // The whole screen is searched, because the middle of it may be
        // covered by another window.
        if (PlatformScreenHasColor(0xFF00FF))
            return true;
    }
    return false;
}

void InitContext(struct Context *ctx) {
    g_open_file_event = SDL_RegisterEvents(1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) FAIL_WITH("can't initialize SDL");
    LOG_INFO("SDL %d.%d.%d initialized", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    PlatformInit(ctx);
    if (ctx->window == NULL) FAIL_WITH("can't create window");

    // The renderer that is used depends on the mode. On Windows the default is
    // the software renderer: the Direct3D renderers of SDL don't composite a
    // window that is a child of the desktop window, so the wallpaper would
    // stay invisible.
    Uint32 renderer_flags = 0;
    if (ctx->renderer_mode == RENDERER_SOFTWARE) {
        renderer_flags = SDL_RENDERER_SOFTWARE;
    } else {
        renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    }
    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, renderer_flags);
    if (ctx->renderer == NULL && ctx->renderer_mode != RENDERER_SOFTWARE) {
        // Fall back to a software renderer; the wallpaper may still work.
        LOG_WARN("can't create an accelerated renderer (%s), falling back to software rendering",
                 SDL_GetError());
        ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (ctx->renderer == NULL) FAIL_WITH("can't create renderer");

    SDL_RendererInfo ri;
    if (SDL_GetRendererInfo(ctx->renderer, &ri) == 0)
        LOG_INFO("renderer: %s (video driver: %s)", ri.name ? ri.name : "unknown",
                 SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");

    // Check that the wallpaper window is really shown on the screen. If it is
    // not, another way of attaching the window is tried, and if that doesn't
    // help either, the software renderer is used.
    if (!PlatformCanCheckScreen()) {
        LOG_INFO("the screen can't be checked on this platform");
    } else {
        int strategies = PlatformWallpaperStrategyCount();
        bool visible = false;

        // A strategy that was chosen explicitly is used as it is.
        int first = 0;
        int last = strategies - 1;
        if (ctx->wallpaper_strategy >= 0) {
            if (ctx->wallpaper_strategy >= strategies)
                FAIL_WITH("there is no strategy %d, use 0 to %d",
                          ctx->wallpaper_strategy, strategies - 1);
            first = last = ctx->wallpaper_strategy;
            LOG_INFO("using the strategy %d (%s), as it was requested",
                     first, PlatformWallpaperStrategyName(first));
        }

        for (int i = first; i <= last && !visible; i++) {
            // The strategies are not tried in the numerical order: the ones
            // that keep the desktop icons and the mouse clicks working come
            // first.
            int strategy = (ctx->wallpaper_strategy >= 0) ? i : PlatformWallpaperStrategyOrder(i);
            if (strategy < 0) break;
            if (i > 0 || ctx->wallpaper_strategy < 0) {
                if (ctx->wallpaper_strategy < 0) LOG_INFO("trying the strategy %d (%s)",
                                                            strategy, PlatformWallpaperStrategyName(strategy));
                if (!PlatformAttachWallpaper(ctx, strategy)) continue;
            }
            if (RendererIsOnScreen(ctx->renderer)) {
                LOG_INFO("the wallpaper is visible on the screen (strategy %d: %s)",
                         strategy, PlatformWallpaperStrategyName(strategy));
                visible = true;
                break;
            }
            LOG_WARN("the strategy %d (%s) doesn't show the wallpaper",
                     strategy, PlatformWallpaperStrategyName(strategy));
        }
        if (!visible) {
            LOG_WARN("the wallpaper window is not visible on the screen");
            if (ctx->renderer_mode != RENDERER_SOFTWARE) {
                LOG_WARN("switching to the software renderer");
                SDL_DestroyRenderer(ctx->renderer);
                ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_SOFTWARE);
                if (ctx->renderer == NULL) FAIL_WITH("can't create the software renderer");
                if (SDL_GetRendererInfo(ctx->renderer, &ri) == 0)
                    LOG_INFO("renderer: %s (video driver: %s)", ri.name ? ri.name : "unknown",
                             SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
                for (int i = first; i <= last && !visible; i++) {
                    int strategy = (ctx->wallpaper_strategy >= 0) ? i : PlatformWallpaperStrategyOrder(i);
                    if (strategy < 0) break;
                    if (i > 0 && !PlatformAttachWallpaper(ctx, strategy)) continue;
                    if (RendererIsOnScreen(ctx->renderer)) {
                        LOG_INFO("the wallpaper is visible on the screen now (strategy %d)", strategy);
                        visible = true;
                        break;
                    }
                }
            }
            if (!visible) {
                // The check of the colour is not reliable: the compositor of
                // Windows can show an old frame, so the wallpaper can be
                // visible even when the check fails. The program keeps running
                // with the strategy it has, because a popup window that covers
                // the desktop icons would be worse.
                LOG_WARN("the wallpaper could not be confirmed on the screen, "
                         "but the program keeps running: the check is not reliable");
            }
        }
    }

    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(ctx->window, &win_w, &win_h);
    LOG_INFO("window size: %dx%d", win_w, win_h);
}

void ClearContext(struct Context *context) {
    if (context->renderer) SDL_DestroyRenderer(context->renderer);
    PlatformCleanup(context);
    SDL_Quit();
    if (context->file) free(context->file);
    if (context->screenshot) free(context->screenshot);
    memset(context, 0, sizeof(*context));
}

void ProcessArguments(int argc, char *argv[], struct Context *context) {
    const char progname[] = "live-paper";
    struct arg_lit *help;
    struct arg_lit *verbose;
    struct arg_lit *stats;
    struct arg_lit *dump_windows;
    struct arg_lit *readback;
    struct arg_str *fit;
    struct arg_str *log_file;
    struct arg_str *renderer;
    struct arg_str *wallpaper_strategy;
    struct arg_str *screenshot;
    struct arg_str *desktop_icons;
    struct arg_str *prepare_desktop;
    struct arg_file *file;
    struct arg_end *end;
    void *argtable[] = {
        help = arg_lit0("h", "help", "= display this help and exit"),
        verbose = arg_lit0("v", "verbose", "= print detailed diagnostics"),
        stats = arg_lit0(NULL, "stats", "= print runtime statistics every few seconds"),
        dump_windows = arg_lit0(NULL, "dump-windows", "= print the structure of the desktop windows and exit"),
        readback = arg_lit0(NULL, "readback", "= read back the rendered pixels (diagnostics only)"),
        log_file = arg_str0(NULL, "log-file", "<path>", "= write diagnostics to the given file"),
        renderer = arg_str0(NULL, "renderer", "<mode>", "= the renderer to use: auto, accelerated or software"),
        wallpaper_strategy = arg_str0(NULL, "wallpaper-strategy", "<n>", "= how the wallpaper is attached to the desktop (0-6)"),
        desktop_icons = arg_str0(NULL, "desktop-icons", "<mode>", "= the layer of the desktop icons: none, redraw, refresh"),
        prepare_desktop = arg_str0(NULL, "prepare-desktop", "<0|1>", "= send the message 0x052c to Progman (Windows only)"),
        screenshot = arg_str0(NULL, "screenshot", "<path>", "= capture the screen into a BMP file (diagnostics only)"),
        fit = arg_str0(NULL, "fit-mode", "<mode>", ""),
        arg_rem(NULL, "= controls the way the wallpaper is fit on screen"),
        arg_rem(NULL, "  possible values: fit, fill, center"),
        file = arg_file0(NULL, NULL, "<file>", "= video or animation file to display"),
        end = arg_end(20),
    };
    if (arg_nullcheck(argtable) != 0) FAIL();
    int nerrors = arg_parse(argc, argv, argtable);
    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        printf("Display a video or an animated file as desktop background.\n\n");
        arg_print_glossary(stdout, argtable, "  %-12s %s\n");
        printf(
            "\n"
            "Examples: %s loop.mp4\n"
            "          %s --fit-mode=fill wallpaper.gif\n"
            "          %s --verbose --stats --log-file=live-paper.log loop.mp4\n",
            progname, progname, progname);
        exit(0);
    }
    if (nerrors > 0) {
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more information.", progname);
        exit(1);
    }

    // Logging must be configured as early as possible, so that everything that
    // happens afterwards is recorded.
    enum LogLevel level = verbose->count > 0 ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO;
    LogInit(level, log_file->count > 0 ? log_file->sval[0] : NULL);
    g_print_stats = stats->count > 0;

    // The dump of the desktop windows doesn't need a file, a window or a
    // video, so it is done before anything else is set up.
    if (dump_windows->count > 0) {
        PlatformDumpWindows();
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        LogShutdown();
        exit(0);
    }

    if (file->count > 0) {
        context->file = strdup(file->filename[0]);
    } else {
        PlatformInitGuiMode(context);
    }
    if (!context->file) {
        printf("%s: missing option <file>\n"
            "Try '%s --help' for more information.", progname, progname);
        exit(1);
    }

    context->fit = FIT_FIT;
    if (fit->count) {
        if (strcmp(fit->sval[0], "fill") == 0) context->fit = FIT_FILL;
        else if (strcmp(fit->sval[0], "center") == 0) context->fit = FIT_CENTER;
    }

    // On Windows the software renderer is used by default: the Direct3D
    // renderers of SDL don't composite a window that is a child of the desktop
    // window, so the wallpaper would stay invisible.
    context->renderer_mode = RENDERER_AUTO;
#ifdef _WIN32
    context->renderer_mode = RENDERER_SOFTWARE;
#endif
    if (renderer->count) {
        if (strcmp(renderer->sval[0], "accelerated") == 0) context->renderer_mode = RENDERER_ACCELERATED;
        else if (strcmp(renderer->sval[0], "software") == 0) context->renderer_mode = RENDERER_SOFTWARE;
        else if (strcmp(renderer->sval[0], "auto") != 0)
            FAIL_WITH("unknown renderer '%s', use auto, accelerated or software", renderer->sval[0]);
    }
    LOG_INFO("the renderer mode is %s",
             context->renderer_mode == RENDERER_SOFTWARE ? "software" :
             context->renderer_mode == RENDERER_ACCELERATED ? "accelerated" : "auto");

    if (screenshot->count > 0) {
        context->screenshot = strdup(screenshot->sval[0]);
        LOG_INFO("the screen will be captured in '%s'", context->screenshot);
    }

    // The way the layer of the desktop icons is prepared.
    if (desktop_icons->count) {
        if (strcmp(desktop_icons->sval[0], "none") == 0) PlatformSetDesktopIconsMode(0);
        else if (strcmp(desktop_icons->sval[0], "redraw") == 0) PlatformSetDesktopIconsMode(1);
        else if (strcmp(desktop_icons->sval[0], "refresh") == 0) PlatformSetDesktopIconsMode(2);
        else FAIL_WITH("the desktop icons mode must be none, redraw or refresh");
        LOG_INFO("the layer of the desktop icons: %s", desktop_icons->sval[0]);
    }
    if (prepare_desktop->count) {
        bool enable = strcmp(prepare_desktop->sval[0], "1") == 0;
        PlatformSetPrepareDesktop(enable);
        LOG_INFO("the message 0x052c is %s", enable ? "sent" : "not sent");
    }

    context->wallpaper_strategy = -1;
    if (wallpaper_strategy->count) {
        char *end = NULL;
        long value = strtol(wallpaper_strategy->sval[0], &end, 10);
        if (!end || *end != '\0' || value < 0 || value >= PlatformWallpaperStrategyCount())
            FAIL_WITH("the wallpaper strategy must be a number from 0 to %d",
                      PlatformWallpaperStrategyCount() - 1);
        context->wallpaper_strategy = (int)value;
    }
#ifdef _WIN32
    // On Windows the strategy 6 is the default: the window is a popup that
    // the mouse clicks pass through, so the video is shown and the clicks
    // reach the desktop. The icons are painted by the shell, below the
    // window.
    if (context->wallpaper_strategy < 0) context->wallpaper_strategy = 6;
#endif

    LOG_INFO("live-paper starting, file='%s'", context->file);
    if (verbose->count > 0) LOG_INFO("verbose diagnostics enabled");
    if (g_print_stats) LOG_INFO("periodic statistics enabled");
    if (readback->count > 0) {
        VideoSetReadback(true);
        LOG_INFO("the rendered pixels are read back for the diagnostics");
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

// Prints the wallpaper status and the media information of the loaded video.
static void LogDiagnostics(const struct Context *context, const struct Video *video) {
    const char *status_str = "unknown";
    switch (context->wallpaper_status) {
        case WALLPAPER_ACTIVE:   status_str = "set"; break;
        case WALLPAPER_INACTIVE: status_str = "not visible"; break;
        case WALLPAPER_FAILED:   status_str = "failed"; break;
        case WALLPAPER_UNKNOWN:  status_str = "unknown"; break;
    }
    LOG_INFO("wallpaper: %s (%s)", status_str, PlatformWallpaperMechanism());
    if (context->wallpaper_detail[0])
        LOG_INFO("wallpaper details: %s", context->wallpaper_detail);

    struct VideoFileInfo info;
    VideoGetFileInfo(video, &info);
    if (info.width > 0) {
        char size[32];
        SysFormatBytes(info.file_size, size, sizeof(size));
        LOG_INFO("media: format=%s codec=%s resolution=%dx%d duration=%.2fs fps=%.2f file_size=%s",
                 info.format, info.codec, info.width, info.height,
                 info.duration_sec, info.frame_rate, size);
    }

    LogMemoryUsage("");
}

// Prints the runtime statistics (fps, memory usage, counters).
static void LogStats(const struct Video *video) {
    struct VideoStats stats;
    VideoGetStats(video, &stats);
    struct MemoryInfo mem;
    char rss[32] = "n/a";
    if (SysGetMemoryInfo(&mem) == 0)
        SysFormatBytes(mem.process_rss, rss, sizeof(rss));
    LOG_INFO("stats: fps=%.1f frames_decoded=%llu frames_rendered=%llu loops=%llu "
             "last_decode=%.1fms rss=%s",
             stats.fps,
             (unsigned long long)stats.frames_decoded,
             (unsigned long long)stats.frames_rendered,
             (unsigned long long)stats.loops,
             stats.last_decode_ms,
             rss);
}

int main(int argc, char *argv[]) {
    // Make the diagnostics visible in the terminal the program was started
    // from (on Windows the program is a GUI application, which normally has
    // no console attached to it).
    LogAttachParentConsole();

    struct Context context = {0};
    ProcessArguments(argc, argv, &context);
    InitContext(&context);
    struct Video* video = VideoLoad(&context);
    LogDiagnostics(&context, video);
    
    double ticks_frequency = (double)SDL_GetPerformanceFrequency();
    uint64_t ticks_now = SDL_GetPerformanceCounter();
    uint64_t ticks_last = 0;
    uint64_t stats_next_ms = SDL_GetTicks64() + 5000;
    uint64_t icons_next_ms = SDL_GetTicks64() + 1000;
    uint64_t screenshot_next_ms = SDL_GetTicks64() + 3000;
    int frames = 0;
    bool quit = false;
    while (!quit) {
        ticks_last = ticks_now;
        ticks_now = SDL_GetPerformanceCounter();
        double delta_sec = (ticks_now - ticks_last)/ticks_frequency;

        VideoUpdate(delta_sec, video, &context);
        PlatformUpdate(&context);

        // The shell can create the snapshot of the desktop again, which would
        // hide the wallpaper: the layer of the desktop icons is made
        // transparent again a few times during the first seconds.
        if (++frames > 10) {
            uint64_t now_ms = SDL_GetTicks64();
            if (now_ms >= icons_next_ms) {
                PlatformRefreshDesktopIcons();
                icons_next_ms = now_ms + 2000;
            }
            // The screen is captured once, when the wallpaper is running: it
            // shows what the user really sees.
            if (context.screenshot && now_ms >= screenshot_next_ms) {
                PlatformCaptureScreen(context.screenshot);
                free(context.screenshot);
                context.screenshot = NULL;
            }
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
                break;
            } else if(event.type == g_open_file_event) {
                LOG_INFO("switching to a new file");
                VideoClear(video, &context);
                free(context.file);
                context.file = event.user.data1;
                video = VideoLoad(&context);
                LogDiagnostics(&context, video);
            }
        }

        if (g_print_stats) {
            uint64_t now_ms = SDL_GetTicks64();
            if (now_ms >= stats_next_ms) {
                LogStats(video);
                stats_next_ms = now_ms + 5000;
            }
        }

        SDL_Delay(1);
    }

    LOG_INFO("shutting down");
    LogStats(video);
    VideoClear(video, &context);
    ClearContext(&context);
    LogShutdown();
    return 0;
}