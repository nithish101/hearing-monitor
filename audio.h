/*
 * audio.h - Audio input (producer) thread interface
 *
 * Declares the entry point for the audio sampling thread.
 * On Raspberry Pi builds, the thread reads from an I2S MEMS microphone
 * via ALSA.  On other platforms, it generates simulated dB readings.
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
 *   1. Read dB from microphone (Pi) or generate simulated value
 *   2. Lock the mutex
 *   3. Write db + set updated flag
 *   4. Unlock the mutex
 *   5. Sleep for SAMPLE_INTERVAL_US
 *   6. Repeat until shared->running == 0
 */
void *audio_thread_func(void *arg);

#endif /* AUDIO_H */
