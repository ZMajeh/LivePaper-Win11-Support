#pragma once

#include <stdbool.h>
#include <stdint.h>

struct Context;

// Runtime statistics of a loaded video, used for diagnostics.
struct VideoStats {
    unsigned long long frames_decoded;  // total frames decoded from the file
    unsigned long long frames_rendered; // total frames drawn to the texture
    unsigned long long decode_errors;  // number of failed decode attempts
    unsigned long long loops;          // number of times the video restarted
    double fps;                        // measured render rate
    double last_decode_ms;             // duration of the last decode
};

// Information about the media file itself.
struct VideoFileInfo {
    char format[64];      // container format, e.g. "gif"
    char codec[64];       // codec name, e.g. "gif"
    int width;
    int height;
    double duration_sec;  // duration of the file, -1 if unknown
    double frame_rate;    // nominal frame rate, 0 if unknown
    uint64_t file_size;   // size of the file on disk, in bytes
};

struct Video* VideoLoad(const struct Context *context);
void VideoUpdate(double delta_sec, struct Video *video, const struct Context *context);
void VideoClear(struct Video *video, const struct Context *context);

// Returns the runtime statistics of the video. video may be NULL.
void VideoGetStats(const struct Video *video, struct VideoStats *stats);

// Enables the diagnostic that reads back the rendered pixels. It disturbs the
// state of the renderer, so it is only meant to be used for the diagnostics.
void VideoSetReadback(bool enabled);

// Returns information about the media file. video may be NULL.
void VideoGetFileInfo(const struct Video *video, struct VideoFileInfo *info);