/*
 * processing.h - Strain computation (consumer) thread interface
 *
 * Declares the entry point for the processing thread.
 * The thread reads dB samples from the shared buffer and computes
 * cumulative strain, normalized score, and time-to-threshold.
 */

#ifndef PROCESSING_H
#define PROCESSING_H

#include "shared.h"

/*
 * processing_thread_func - Consumer thread entry point
 *
 * @param arg   Pointer to a SharedData structure (cast from void*)
 * @return      NULL on thread exit
 *
 * Behavior:
 *   1. Lock the mutex
 *   2. If updated flag is set:
 *      a. Read db value
 *      b. Accumulate strain:  strain += db * dt
 *      c. Normalize score:    score = (strain / threshold) * 100, capped at 100
 *      d. Estimate time left: time_left = (threshold - strain) / db
 *      e. Clear updated flag
 *   3. Unlock the mutex
 *   4. Sleep briefly
 *   5. Repeat until shared->running == 0
 */
void *processing_thread_func(void *arg);

#endif /* PROCESSING_H */
