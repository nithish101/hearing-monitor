/*
 * audio.c - Audio Input (Producer) Thread
 *
 * This module implements the producer side of the producer-consumer pattern.
 * It simulates a microphone by generating realistic dB readings using an
 * exponential moving average (EMA) to smooth random fluctuations — mimicking
 * the way real environmental noise behaves (gradual changes with occasional
 * spikes, not pure random jumps every second).
 *
 * Concurrency notes:
 *   - Writes to shared->db and shared->updated are always done while
 *     holding the mutex lock.
 *   - The lock is held for the minimum time necessary (lock → write → unlock)
 *     to avoid blocking the consumer thread.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "audio.h"

/* ── Simulation Parameters ──────────────────────────────────────────── */

#define DB_MIN          55.0f   /* quietest simulated level (library)     */
#define DB_MAX         105.0f   /* loudest simulated level  (rock concert)*/
#define EMA_ALPHA        0.3f   /* smoothing factor (0 = static, 1 = raw) */
#define FLUCTUATION      8      /* max ±random fluctuation per sample     */

/* ── Helper: generate one simulated dB reading ──────────────────────── */

/*
 * simulate_db - produce a smoothed simulated decibel reading
 *
 * On the first call (*prev_db == 0) the function seeds with a random
 * value in the [DB_MIN, DB_MAX] range.  Subsequent calls apply a small
 * random perturbation and smooth it with an EMA to avoid jarring jumps.
 *
 * @param prev_db  In/out pointer to the previous dB value (state)
 * @return         The new simulated dB value
 */
static float simulate_db(float *prev_db)
{
    float raw;

    if (*prev_db == 0.0f) {
        /* First sample: seed with a random value in the middle range */
        raw = DB_MIN + (float)(rand() % (int)(DB_MAX - DB_MIN));
    } else {
        /* Subsequent samples: perturb ±FLUCTUATION around previous */
        int delta = (rand() % (2 * FLUCTUATION + 1)) - FLUCTUATION;
        raw = *prev_db + (float)delta;
    }

    /* Clamp to valid range */
    if (raw < DB_MIN) raw = DB_MIN;
    if (raw > DB_MAX) raw = DB_MAX;

    /* Exponential moving average for natural smoothing */
    *prev_db = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * (*prev_db);
    return *prev_db;
}

/* ── Thread Entry Point ─────────────────────────────────────────────── */

void *audio_thread_func(void *arg)
{
    SharedData *shared = (SharedData *)arg;
    float prev_db = 0.0f;

    while (1) {
        float db = simulate_db(&prev_db);

        /* ── Critical section: write sample to shared buffer ──────── */
        pthread_mutex_lock(&shared->lock);

        if (!shared->running) {
            pthread_mutex_unlock(&shared->lock);
            break;
        }

        shared->db      = db;
        shared->updated  = 1;       /* signal consumer */

        pthread_mutex_unlock(&shared->lock);
        /* ── End critical section ─────────────────────────────────── */

        usleep(SAMPLE_INTERVAL_US);
    }

    return NULL;
}
