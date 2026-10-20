#include "video.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

#include "fail.h"
#include "log.h"
#include "platform.h"
struct CacheEntry {
    SDL_Texture *tex;
    double frame_end_time;
};

struct Video {
    AVCodecContext *decoder_ctx;
    AVFormatContext *input_ctx;
    struct SwsContext *sws_ctx;
    AVFrame *sws_frame;
    int stream_id;
    double time_base;

    double time;
    double next_frame_time;

    SDL_Texture *tex;
    SDL_Rect tex_dest;

    // diagnostics
    struct VideoStats stats;
    struct VideoFileInfo file_info;
    uint64_t fps_window_start_ms;
    unsigned long long fps_window_frames;
    Uint64 decode_start_ms;   // a tick count, not a duration
};

static bool DecodeNextFrame(AVFrame *frame, double *frame_end_time, struct Video *v) {
    for (;;) {
        AVPacket packet;
        int ret = av_read_frame(v->input_ctx, &packet);
        if (ret < 0 && ret != AVERROR_EOF) FAIL_WITH("%s", av_err2str(ret));
        if (ret == AVERROR_EOF) { // reached the end of file
            av_packet_unref(&packet);
            return false;
        }

        if (packet.stream_index == v->stream_id) {
            // for video streams 1 packet == 1 frame
            int send_ret = avcodec_send_packet(v->decoder_ctx, &packet);
            // The decoder keeps its own reference to the data of the packet
            // (it is shared with the one of this packet), so the reference of
            // this packet can be released. Without this the buffer of the
            // packet is leaked for every decoded frame.
            av_packet_unref(&packet);
            if (send_ret < 0) FAIL_WITH("%s", av_err2str(send_ret));
            int ret = avcodec_receive_frame(v->decoder_ctx, frame);
            if (ret < 0 && ret != AVERROR(EAGAIN)) FAIL_WITH("%s", av_err2str(ret));
            if (ret == AVERROR(EAGAIN)) {
                av_frame_unref(frame);
                av_packet_unref(&packet);
                continue; // read frame again
            }

            *frame_end_time = frame->best_effort_timestamp*v->time_base + frame->duration*av_q2d(frame->time_base);
            return true;
        }
        av_packet_unref(&packet);
    }
}

// Returns the size of the file in bytes, or 0 if it can't be determined.
static uint64_t GetFileSize(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint64_t size = 0;
    if (fseek(f, 0, SEEK_END) == 0) {
        long pos = ftell(f);
        if (pos > 0) size = (uint64_t)pos;
    }
    fclose(f);
    return size;
}

// Checks that the file exists and can be read. Returns 0 on success,
// -1 otherwise; on failure the reason is written to err (if not NULL).
static int CheckFileReadable(const char *path, char *err, size_t err_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, err_size, "can't open file for reading");
        return -1;
    }
    // Make sure we can actually read from the file, not just open it.
    char probe[4096];
    size_t n = fread(probe, 1, sizeof(probe), f);
    if (n == 0 && ferror(f)) {
        snprintf(err, err_size, "can't read from file");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

struct Video *VideoLoad(const struct Context *ctx) {
    struct Video *v = av_mallocz(sizeof(struct Video));

    char read_err[128] = "";
    if (CheckFileReadable(ctx->file, read_err, sizeof(read_err)) != 0)
        FAIL_WITH("Can't read file '%s': %s", ctx->file, read_err);
    LOG_INFO("loading file '%s'", ctx->file);

    if (avformat_open_input(&v->input_ctx, ctx->file, NULL, NULL) != 0)
        FAIL_WITH("Can't open file '%s'", ctx->file);
    if (avformat_find_stream_info(v->input_ctx, NULL) < 0)
        FAIL_WITH("Can't find stream information");
    const AVCodec *codec = NULL;
    v->stream_id = av_find_best_stream(v->input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (v->stream_id < 0) FAIL_WITH("Can't find a video stream in '%s'", ctx->file);

    v->decoder_ctx = avcodec_alloc_context3(codec);
    AVStream *stream = v->input_ctx->streams[v->stream_id];
    avcodec_parameters_to_context(v->decoder_ctx, stream->codecpar);
    if (avcodec_open2(v->decoder_ctx, codec, NULL) < 0)
        FAIL();
    v->time_base = av_q2d(stream->time_base);

    int w = v->decoder_ctx->width;
    int h = v->decoder_ctx->height;
    if (w <= 0 || h <= 0)
        FAIL_WITH("Invalid video dimensions %ix%i in '%s'", w, h, ctx->file);
    if (v->decoder_ctx->pix_fmt == AV_PIX_FMT_NONE)
        FAIL_WITH("Unknown pixel format in '%s'; the file may be corrupted or incomplete", ctx->file);

    enum AVPixelFormat target_fmt = AV_PIX_FMT_RGB24;
    v->sws_ctx = sws_getContext( // convert to RGB24, no resize
        w, h, v->decoder_ctx->pix_fmt,
        w, h, target_fmt,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);
    if (v->sws_ctx == NULL)
        FAIL_WITH("Can't convert '%s' (%ix%i, pixel format %s) to RGB24", ctx->file, w, h,
                  av_get_pix_fmt_name(v->decoder_ctx->pix_fmt));
    v->sws_frame = av_frame_alloc();
    v->sws_frame->format = target_fmt;
    v->sws_frame->width = w;
    v->sws_frame->height = h;
    if (av_frame_get_buffer(v->sws_frame, 32))
        FAIL();

    int win_w, win_h;
    SDL_GetWindowSize(ctx->window, &win_w, &win_h);
    float aspect_ratio = 1.0;
    if (ctx->fit == FIT_FIT) aspect_ratio = fminf((float)win_w/w, (float)win_h/h);
    else if (ctx->fit == FIT_FILL) aspect_ratio = fmaxf((float)win_w/w, (float)win_h/h);
    const int target_w = (int)(w * aspect_ratio);
    const int target_h = (int)(h * aspect_ratio);
    const SDL_Rect dest_rect = {
        .x = (win_w - target_w)/2,
        .y = (win_h - target_h)/2,
        .w = target_w,
        .h = target_h
    };
    v->tex_dest = dest_rect;
    v->tex = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!v->tex) FAIL_WITH("can't create SDL texture %i x %i pix", w, h);

    // collect information about the media file for diagnostics
    snprintf(v->file_info.format, sizeof(v->file_info.format), "%s",
             v->input_ctx->iformat->name ? v->input_ctx->iformat->name : "unknown");
    snprintf(v->file_info.codec, sizeof(v->file_info.codec), "%s", codec->name ? codec->name : "unknown");
    v->file_info.width = w;
    v->file_info.height = h;
    // Note: since FFmpeg 5.x the duration of the container is expressed in
    // AV_TIME_BASE units rather than in the stream time base.
    v->file_info.duration_sec = v->input_ctx->duration != AV_NOPTS_VALUE
        ? (double)v->input_ctx->duration / (double)AV_TIME_BASE : -1.0;
    AVRational fr = av_guess_frame_rate(v->input_ctx, stream, NULL);
    v->file_info.frame_rate = (fr.num > 0 && fr.den > 0) ? av_q2d(fr) : 0.0;
    v->file_info.file_size = GetFileSize(ctx->file);

    LOG_INFO("loaded '%s': format=%s codec=%s size=%dx%d duration=%.2fs fps=%.2f file_size=%llu",
             ctx->file, v->file_info.format, v->file_info.codec, w, h,
             v->file_info.duration_sec, v->file_info.frame_rate,
             (unsigned long long)v->file_info.file_size);
    LOG_INFO("textures: %ix%i, destination rect %ix%i at (%i, %i)",
             w, h, dest_rect.w, dest_rect.h, dest_rect.x, dest_rect.y);

    v->fps_window_start_ms = SDL_GetTicks64();
    return v;
}

static void ResetVideo(struct Video *v) {
    avcodec_flush_buffers(v->decoder_ctx);
    av_seek_frame(v->input_ctx, v->stream_id, 0, AVSEEK_FLAG_BACKWARD);
}

// One-time diagnostic: reads back a few pixels from the renderer and checks
// the decoded frame, to find out whether the video is actually drawn into the
// texture or not. It is only done when it is enabled explicitly, because the
// read-back may disturb the state of the renderer.
static bool g_readback_done = false;
static bool g_readback_enabled = false;

void VideoSetReadback(bool enabled) {
    g_readback_enabled = enabled;
}

static void LogRenderedPixel(const struct Context *ctx, struct Video *v) {
    if (!g_readback_enabled || g_readback_done) return;
    g_readback_done = true;

    // The decoded frame itself: is there anything in it?
    const uint8_t *src = v->sws_frame->data[0];
    int src_pitch = v->sws_frame->linesize[0];
    int src_w = v->sws_frame->width, src_h = v->sws_frame->height;
    if (src && src_w > 0 && src_h > 0) {
        const uint8_t *p = src + (src_h / 2) * src_pitch + (src_w / 2) * 3;
        LOG_INFO("the decoded frame is %dx%d, its middle pixel is R=%u G=%u B=%u",
                 src_w, src_h, p[0], p[1], p[2]);
    }

    // A grid of pixels from the renderer.
    Uint32 pixels[9];
    memset(pixels, 0, sizeof(pixels));
    int x = v->tex_dest.x, y = v->tex_dest.y, w = v->tex_dest.w, h = v->tex_dest.h;
    for (int i = 0; i < 9; i++) {
        SDL_Rect rect = {
            .x = x + (w * (i % 3)) / 3,
            .y = y + (h * (i / 3)) / 3,
            .w = 1,
            .h = 1,
        };
        if (SDL_RenderReadPixels(ctx->renderer, &rect, SDL_PIXELFORMAT_ARGB8888,
                                 &pixels[i], 4) != 0) {
            LOG_WARN("can't read back the rendered pixels: %s", SDL_GetError());
            return;
        }
    }
    bool all_black = true;
    for (int i = 0; i < 9; i++) {
        Uint32 p = pixels[i];
        LOG_INFO("  pixel %d: R=%u G=%u B=%u A=%u", i,
                 (p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF, (p >> 24) & 0xFF);
        if ((p & 0xFFFFFF) != 0) all_black = false;
    }
    if (all_black)
        LOG_WARN("all the sampled pixels are black: the renderer doesn't show the video");
    else
        LOG_INFO("the renderer shows the video");
}

void VideoUpdate(double delta_sec, struct Video *v, const struct Context *ctx) {
    assert(v);
    assert(v->input_ctx);

    v->time += delta_sec;
    if (v->time < v->next_frame_time) {  // previously rendered frame is still valid, nothing to do here
        return;
    }

    for (bool found_frame = false; !found_frame;) {
        AVFrame frame = {0};
        double frame_end_time = 0.0;
        if (!DecodeNextFrame(&frame, &frame_end_time, v)) {
            // no frames left (EOF), restart the video
            LOG_INFO("end of file reached, restarting the video (loop %llu)",
                     (unsigned long long)(v->stats.loops + 1));
            ResetVideo(v);
            v->time = 0.0;
            v->next_frame_time = -1.0;
            v->stats.loops++;
            continue;
        }
        if (frame_end_time < v->next_frame_time) FAIL();  // something wrong with frame order
        if (frame_end_time > v->time) {
            found_frame = true;
            v->next_frame_time = frame_end_time;
            v->stats.frames_decoded++;
            v->decode_start_ms = SDL_GetTicks64();

            sws_scale(v->sws_ctx, (uint8_t const *const *)frame.data, frame.linesize, 0, v->decoder_ctx->height,
                      v->sws_frame->data, v->sws_frame->linesize);

            // Upload the decoded frame into the texture. SDL_UpdateTexture() is
            // used instead of SDL_LockTexture()/SDL_UnlockTexture(): it is the
            // path that every renderer supports, and the Direct3D renderers of
            // SDL don't upload the data of a locked texture.
            if (SDL_UpdateTexture(v->tex, NULL, v->sws_frame->data[0],
                                  v->sws_frame->linesize[0]) != 0)
                FAIL_WITH("can't update the texture: %s", SDL_GetError());

            v->stats.frames_rendered++;
            v->stats.last_decode_ms = (double)(SDL_GetTicks64() - v->decode_start_ms);
            v->fps_window_frames++;
        }

        av_frame_unref(&frame);
    }

    // Update the measured frame rate once per second.
    uint64_t now_ms = SDL_GetTicks64();
    if (now_ms - v->fps_window_start_ms >= 1000) {
        double elapsed = (double)(now_ms - v->fps_window_start_ms) / 1000.0;
        v->stats.fps = (double)v->fps_window_frames / elapsed;
        v->fps_window_start_ms = now_ms;
        v->fps_window_frames = 0;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderCopy(ctx->renderer, v->tex, NULL, &v->tex_dest);
    LogRenderedPixel(ctx, v);
    SDL_RenderPresent(ctx->renderer);
}

void VideoGetStats(const struct Video *v, struct VideoStats *stats) {
    if (!v) {
        memset(stats, 0, sizeof(*stats));
        return;
    }
    *stats = v->stats;
}

void VideoGetFileInfo(const struct Video *v, struct VideoFileInfo *info) {
    if (!v) {
        memset(info, 0, sizeof(*info));
        info->duration_sec = -1.0;
        return;
    }
    *info = v->file_info;
}

void VideoClear(struct Video *v, const struct Context *ctx) {
    if (v->tex) SDL_DestroyTexture(v->tex);
    av_frame_free(&v->sws_frame);
    sws_freeContext(v->sws_ctx);
    avcodec_free_context(&v->decoder_ctx);
    avformat_close_input(&v->input_ctx);
    memset(v, 0, sizeof(*v));
    av_free(v);
}
