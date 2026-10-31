/*****************************************************************************
 * AmigaOS audio output through AHI or Paula audio.device: a double-buffered
 * stream consumed by a playback process. The mixer runs on the main task
 * and fills a shared Fast-RAM ring; no C++ callback runs on the audio task.
 *****************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Fill callback: write exactly `bytes` bytes of interleaved signed 16-bit big-endian stereo into `dst`. */
typedef void (*amiga_audio_fill_fn)(void* user, unsigned char* dst, int bytes);

/* Device probing is deferred to open, in the owning playback process. */
int amiga_audio_available(void);
const char* amiga_audio_backend(void);

/* OPENRCT2_AUDIO=PAULA or AHI forces a backend; default tries AHI then Paula.
 * Open for `freq` Hz stereo 16-bit output with `numBuffers` buffers of `frames` frames each.
 * Returns 1 on success. */
int amiga_audio_open(int freq, int frames, int numBuffers);

/* Refill and queue every buffer that has finished playing. Returns the number of buffers refilled. */
int amiga_audio_pump(amiga_audio_fill_fn fill, void* user);

/* Stop playback and free everything. Safe to call when not open. */
void amiga_audio_close(void);

/* Diagnostics: buffers written / underruns (gaps where all buffers had drained). */
void amiga_audio_stats(unsigned* written, unsigned* underruns);
/* dos GetVar wrapper: 1 if the variable exists (value copied to buf). */
int amiga_audio_getenv(const char* name, char* buf, int len);

#ifdef __cplusplus
}
#endif
