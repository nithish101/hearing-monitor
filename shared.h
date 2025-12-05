/*
 * shared.h - Shared data structures and constants for the Hearing Strain Monitor
 *
 * This header defines the shared buffer used in the producer-consumer pattern
 * between the audio sampling thread and the processing thread. Access to shared
 * state is protected by a pthread mutex to prevent race conditions.
 */

#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

/* ── Threshold & Timing Constants ───────────────────────────────────── */

/*
 * STRAIN_THRESHOLD represents the maximum cumulative strain before
 * "temporary hearing fatigue" is reached.  The value is unitless and
 * chosen so that continuous exposure to 85 dB (OSHA action level) for
 * ~8 hours maps roughly to 100%.
 *
 *   85 dB × 3600 s/hr × 8 hr ≈ 2,448,000
 *
 * We round to 2,400,000 for clarity.
 */
#define STRAIN_THRESHOLD  2400000.0f

/*
 * SAMPLE_INTERVAL_US is the sleep duration (in microseconds) between
 * audio samples in the producer thread.  1 000 000 µs = 1 Hz sampling.
 */
#define SAMPLE_INTERVAL_US  1000000

/*
 * DISPLAY_INTERVAL_US is the sleep duration (in microseconds) between
 * dashboard refreshes in the main display loop.
 */
#define DISPLAY_INTERVAL_US  1000000

/* ── Alert Thresholds ───────────────────────────────────────────────── */

#define ALERT_WARNING   60.0f   /* score ≥ 60 → yellow warning   */
#define ALERT_DANGER    80.0f   /* score ≥ 80 → red danger alert  */

/* ── Shared Data ────────────────────────────────────────────────────── */

/*
 * SharedData is the central communication structure between threads.
 *
 * Fields:
 *   db        – latest decibel reading produced by the audio thread
 *   strain    – cumulative strain value updated by the processing thread
 *   score     – normalized strain score (0–100)
 *   time_left – estimated seconds until threshold is reached
 *   updated   – flag: 1 when audio thread has written a new sample that
 *               the processing thread has not yet consumed
 *   running   – flag: 0 signals all threads to exit their loops
 *   lock      – mutex protecting every field above
 */
typedef struct {
    float            db;
    float            strain;
    float            score;
    float            time_left;
    int              updated;
    int              running;
    pthread_mutex_t  lock;
} SharedData;

#endif /* SHARED_H */
