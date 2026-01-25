/*
 * audio.c - Audio Input (Producer) Thread
 *
 * This module implements the producer side of the producer-consumer pattern.
 *
 * On Raspberry Pi (RASPBERRY_PI defined):
 *   Reads raw PCM audio from an I2S MEMS microphone via ALSA, computes
 *   the RMS amplitude of each captured buffer, and converts it to a
 *   decibel value using 20 * log10(rms / reference).
 *
 * On other platforms (simulation mode):
 *   Generates realistic dB readings using an exponential moving average
 *   to smooth random fluctuations.
 *
 * Concurrency notes:
 *   - Writes to shared->db and shared->updated are always done while
 *     holding the mutex lock.
 *   - The lock is held for the minimum time necessary (lock -> write -> unlock)
 *     to avoid blocking the consumer thread.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#ifdef RASPBERRY_PI
#include <alsa/asoundlib.h>
#endif

#include "audio.h"

/* ══════════════════════════════════════════════════════════════════════
 *  RASPBERRY PI: ALSA I2S Microphone Capture
 * ══════════════════════════════════════════════════════════════════════ */

#ifdef RASPBERRY_PI

/*
 * Reference amplitude for dB conversion.  For a 16-bit signed signal
 * the theoretical maximum amplitude is 32768.  We use this as 0 dBFS
 * (decibels relative to full scale).
 */
#define DB_REF  32768.0f

/*
 * Floor value to prevent log10(0) when the mic reads silence.
 */
#define DB_FLOOR  30.0f

/*
 * alsa_init - open and configure the ALSA capture device
 *
 * @param handle_out  Receives the opened PCM handle
 * @return            0 on success, -1 on failure
 */
static int alsa_init(snd_pcm_t **handle_out)
{
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int rate = ALSA_RATE;
    int dir;

    if (snd_pcm_open(&handle, ALSA_DEVICE, SND_PCM_STREAM_CAPTURE, 0) < 0) {
        fprintf(stderr, "audio: failed to open ALSA device %s\n", ALSA_DEVICE);
        return -1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, ALSA_CHANNELS);
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir);

    snd_pcm_uframes_t frames = ALSA_FRAMES;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    if (snd_pcm_hw_params(handle, params) < 0) {
        fprintf(stderr, "audio: failed to set ALSA hardware parameters\n");
        snd_pcm_close(handle);
        return -1;
    }

    *handle_out = handle;
    return 0;
}

/*
 * compute_db - calculate dB SPL from a buffer of 16-bit PCM samples
 *
 * Steps:
 *   1. Compute sum of squares across all samples
 *   2. Divide by sample count to get mean square
 *   3. Take square root for RMS amplitude
 *   4. Convert to decibels: 20 * log10(rms / reference)
 *   5. Clamp to DB_FLOOR to avoid nonsensical negative values
 */
static float compute_db(const int16_t *buf, int nframes)
{
    double sum_sq = 0.0;

    for (int i = 0; i < nframes; i++) {
        double sample = (double)buf[i];
        sum_sq += sample * sample;
    }

    double rms = sqrt(sum_sq / (double)nframes);

    if (rms < 1.0)
        return DB_FLOOR;

    float db = 20.0f * log10f((float)(rms / DB_REF)) + 94.0f;

    /* 94 dBSPL offset: 0 dBFS ≈ 94 dBSPL for a typical MEMS mic */
    if (db < DB_FLOOR) db = DB_FLOOR;

    return db;
}

/* ── Thread Entry Point (ALSA) ──────────────────────────────────────── */

void *audio_thread_func(void *arg)
{
    SharedData *shared = (SharedData *)arg;
    snd_pcm_t *handle = NULL;

    if (alsa_init(&handle) != 0) {
        fprintf(stderr, "audio: ALSA init failed, thread exiting\n");
        pthread_mutex_lock(&shared->lock);
        shared->running = 0;
        pthread_mutex_unlock(&shared->lock);
        return NULL;
    }

    int16_t *buffer = malloc(ALSA_FRAMES * ALSA_CHANNELS * sizeof(int16_t));
    if (!buffer) {
        fprintf(stderr, "audio: malloc failed\n");
        snd_pcm_close(handle);
        return NULL;
    }

    while (1) {
        snd_pcm_sframes_t rc = snd_pcm_readi(handle, buffer, ALSA_FRAMES);

        if (rc == -EPIPE) {
            /* Buffer overrun — recover and try again */
            snd_pcm_prepare(handle);
            continue;
        } else if (rc < 0) {
            fprintf(stderr, "audio: read error: %s\n", snd_strerror((int)rc));
            break;
        }

        float db = compute_db(buffer, (int)rc);

        /* ── Critical section: write sample to shared buffer ──────── */
        pthread_mutex_lock(&shared->lock);

        if (!shared->running) {
            pthread_mutex_unlock(&shared->lock);
            break;
        }

        shared->db      = db;
        shared->updated  = 1;

        pthread_mutex_unlock(&shared->lock);
        /* ── End critical section ─────────────────────────────────── */

        usleep(SAMPLE_INTERVAL_US);
    }

    free(buffer);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    return NULL;
}

#else /* !RASPBERRY_PI — simulation mode */

/* ══════════════════════════════════════════════════════════════════════
 *  SIMULATION: generate realistic dB values for testing
 * ══════════════════════════════════════════════════════════════════════ */

#define DB_MIN          55.0f
#define DB_MAX         105.0f
#define EMA_ALPHA        0.3f
#define FLUCTUATION      8

static float simulate_db(float *prev_db)
{
    float raw;

    if (*prev_db == 0.0f) {
        raw = DB_MIN + (float)(rand() % (int)(DB_MAX - DB_MIN));
    } else {
        int delta = (rand() % (2 * FLUCTUATION + 1)) - FLUCTUATION;
        raw = *prev_db + (float)delta;
    }

    if (raw < DB_MIN) raw = DB_MIN;
    if (raw > DB_MAX) raw = DB_MAX;

    *prev_db = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * (*prev_db);
    return *prev_db;
}

void *audio_thread_func(void *arg)
{
    SharedData *shared = (SharedData *)arg;
    float prev_db = 0.0f;

    while (1) {
        float db = simulate_db(&prev_db);

        pthread_mutex_lock(&shared->lock);

        if (!shared->running) {
            pthread_mutex_unlock(&shared->lock);
            break;
        }

        shared->db      = db;
        shared->updated  = 1;

        pthread_mutex_unlock(&shared->lock);

        usleep(SAMPLE_INTERVAL_US);
    }

    return NULL;
}

#endif /* RASPBERRY_PI */
