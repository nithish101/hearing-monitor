/*
 * processing.c - Strain Computation (Consumer) Thread
 *
 * This module implements the consumer side of the producer-consumer pattern.
 * It reads dB samples deposited by the audio thread and computes:
 *
 *   1. Cumulative strain   — strain += db × dt  (energy-over-time model)
 *   2. Normalized score    — (strain / threshold) × 100, capped at 100
 *   3. Time-to-threshold   — (threshold - strain) / db, with edge-case handling
 *
 * The "updated" flag in SharedData acts as a single-slot buffer indicator:
 * the consumer only processes when the producer has written a fresh sample.
 * This avoids double-counting the same dB reading.
 *
 * Concurrency notes:
 *   - All reads of shared->db and writes to shared->strain / score / time_left
 *     are performed under the mutex.
 *   - The consumer polls at ~100 ms intervals (much faster than the 1 Hz
 *     producer) so it picks up new samples promptly without busy-waiting.
 */

#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "processing.h"

/* ── Constants ──────────────────────────────────────────────────────── */

#define DT              1.0f            /* time step in seconds                */
#define POLL_INTERVAL   100000          /* 100 ms polling interval (µs)        */
#define INFINITY_TIME   99999.0f        /* sentinel for "effectively forever"  */

/* ── Thread Entry Point ─────────────────────────────────────────────── */

void *processing_thread_func(void *arg)
{
    SharedData *shared = (SharedData *)arg;

    while (1) {
        pthread_mutex_lock(&shared->lock);

        if (!shared->running) {
            pthread_mutex_unlock(&shared->lock);
            break;
        }

        if (shared->updated) {
            float db = shared->db;

            /* ── Strain accumulation ──────────────────────────────── */
            /*
             *  strain += db × dt
             *
             * This is a simplified energy-dose model: louder sounds
             * contribute proportionally more strain per unit time.
             * A proper model would use 10^(db/10) weighting, but
             * the linear model is easier to explain in interviews
             * and still demonstrates the systems design concepts.
             */
            shared->strain += db * DT;

            /* ── Normalize to 0–100 score ─────────────────────────── */
            shared->score = (shared->strain / STRAIN_THRESHOLD) * 100.0f;
            if (shared->score > 100.0f)
                shared->score = 100.0f;

            /* ── Time-to-threshold estimate ───────────────────────── */
            if (shared->score >= 100.0f) {
                /* Already at or past threshold */
                shared->time_left = 0.0f;
            } else if (db < 1.0f) {
                /* Near-silent: threshold effectively unreachable */
                shared->time_left = INFINITY_TIME;
            } else {
                /*
                 * Estimate assuming current dB level persists:
                 *   remaining_strain = threshold - current_strain
                 *   time = remaining_strain / db        (seconds)
                 */
                shared->time_left = (STRAIN_THRESHOLD - shared->strain) / db;
            }

            shared->updated = 0;        /* mark sample as consumed */
        }

        pthread_mutex_unlock(&shared->lock);

        usleep(POLL_INTERVAL);
    }

    return NULL;
}
