/*
 * audio.h - Audio input (producer) thread interface
 *
 * Declares the entry point for the audio sampling thread.
 * The thread continuously generates simulated dB readings and writes
 * them into the shared buffer under mutex protection.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "shared.h"

/*
 * audio_thread_func - Producer thread entry point
 *
 * @param arg   Pointer to a SharedData structure (cast from void*)
 * @return      NULL on thread exit
 *
 * Behavior:
 *   1. Generate a simulated dB reading (60–100 range with smoothing)
 *   2. Lock the mutex
 *   3. Write db + set updated flag
 *   4. Unlock the mutex
 *   5. Sleep for SAMPLE_INTERVAL_US
 *   6. Repeat until shared->running == 0
 */
void *audio_thread_func(void *arg);

#endif /* AUDIO_H */
