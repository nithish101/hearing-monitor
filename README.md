# 🎧 Hearing Strain Monitor

A **real-time, multi-threaded hearing strain monitoring system** built in C.

This project demonstrates **concurrent systems design** using a producer-consumer architecture with POSIX threads and mutex synchronization — disguised as an audio health application.

## Architecture

```
┌─────────────────────┐        ┌───────────────┐        ┌──────────────┐
│  Audio Thread        │──dB──▶│ Shared Buffer  │──────▶│  Processing  │
│  (producer, 1 Hz)   │        │  (mutex-locked)│        │  Thread      │
└─────────────────────┘        └───────┬───────┘        └──────────────┘
                                       │
                                       ▼
                               ┌───────────────┐
                               │  Main Thread   │
                               │  (display, 1s) │
                               └───────────────┘
```

| Thread     | Role              | Responsibility                              |
|------------|-------------------|---------------------------------------------|
| Thread 1   | Producer          | Generates simulated dB readings (1 Hz)      |
| Thread 2   | Consumer          | Computes strain score & time-to-threshold    |
| Main       | Display           | Renders real-time dashboard to terminal      |

## Strain Model

```c
strain += db × dt;                          // cumulative dose
score   = (strain / threshold) × 100;      // normalized 0–100
time_remaining = (threshold - strain) / db; // estimated seconds left
```

**Threshold** is calibrated so that continuous 85 dB exposure for ~8 hours maps to 100% strain (OSHA action level guideline).

## Concurrency Design

- **Shared buffer** (`SharedData`) is protected by a `pthread_mutex_t`
- **Producer** locks → writes `db` + sets `updated` flag → unlocks
- **Consumer** locks → reads `db` (only if `updated`) → computes → writes results → unlocks
- **Display** locks → reads `score`, `time_left`, `db` → unlocks → renders
- **Graceful shutdown** via `SIGINT` handler that sets `running = 0` under lock

## Building & Running

```bash
make          # compile
make run      # compile + run
make clean    # remove build artifacts
```

Press **Ctrl-C** to stop monitoring. The program will cleanly join threads and print final statistics.

## File Structure

```
├── main.c          # Entry point, display loop, signal handling
├── audio.c         # Producer thread: simulated dB generation
├── audio.h         # Producer interface
├── processing.c    # Consumer thread: strain computation
├── processing.h    # Consumer interface
├── shared.h        # Shared data structures & constants
├── Makefile         # Build system
└── README.md
```

## Key Interview Talking Points

- **Architecture**: "Producer-consumer with separate threads for data ingestion and processing"
- **Concurrency**: "Mutex protects shared memory to prevent race conditions between threads"
- **Design Choice**: "Thread separation ensures responsiveness and scalability"
- **Tradeoffs**: "Simplified strain model to focus on systems design rather than DSP accuracy"
- **Shutdown**: "SIGINT handler sets a flag under lock; threads check and exit on next loop iteration"

## License

MIT
