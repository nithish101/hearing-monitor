/*
 * main.c - Hearing Strain Monitor: Entry Point & Display Loop
 *
 * ┌─────────────────────┐        ┌───────────────┐        ┌──────────────┐
 * │  Audio Thread        │──dB──▶│ Shared Buffer  │──────▶│  Processing  │
 * │  (producer, 1 Hz)   │        │  (mutex-locked)│        │  Thread      │
 * └─────────────────────┘        └───────┬───────┘        └──────────────┘
 *                                        │
 *                                        ▼
 *                                ┌───────────────┐
 *                                │  Main Thread   │
 *                                │  (display, 1s) │
 *                                └───────────────┘
 *                                        │
 *                                        ▼
 *                                ┌───────────────┐
 *                                │  GPIO Output   │
 *                                │  (LEDs/buzzer) │
 *                                └───────────────┘
 *
 * This file:
 *   1. Initializes the shared data, mutex, and GPIO (on Pi)
 *   2. Spawns the audio producer and processing consumer threads
 *   3. Runs a 1-second display loop that prints a real-time dashboard
 *      and drives GPIO indicators
 *   4. On SIGINT (Ctrl-C), signals threads to stop and joins them
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "shared.h"
#include "audio.h"
#include "processing.h"
#include "gpio.h"

/* ── ANSI Color Codes ───────────────────────────────────────────────── */

#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BG_RED      "\033[41m"
#define CLEAR_LINE  "\033[2K"

/* ── Global shared state (so signal handler can reach it) ───────────── */

static SharedData shared;

/* ── Signal Handler ─────────────────────────────────────────────────── */

/*
 * handle_sigint - graceful shutdown on Ctrl-C
 *
 * Sets shared.running to 0 under mutex protection so that both worker
 * threads see the flag and exit their loops on the next iteration.
 */
static void handle_sigint(int sig)
{
    (void)sig;
    pthread_mutex_lock(&shared.lock);
    shared.running = 0;
    pthread_mutex_unlock(&shared.lock);
}

/* ── Display Helpers ────────────────────────────────────────────────── */

static void format_time(float seconds, char *buf, size_t bufsize)
{
    if (seconds >= 99999.0f) {
        snprintf(buf, bufsize, "  ∞");
        return;
    }
    int s = (int)seconds;
    int h = s / 3600;
    int m = (s % 3600) / 60;
    int sec = s % 60;

    if (h > 0)
        snprintf(buf, bufsize, "%dh %dm %ds", h, m, sec);
    else if (m > 0)
        snprintf(buf, bufsize, "%dm %ds", m, sec);
    else
        snprintf(buf, bufsize, "%ds", sec);
}

static const char *score_color(float score)
{
    if (score >= ALERT_DANGER)  return RED;
    if (score >= ALERT_WARNING) return YELLOW;
    return GREEN;
}

static const char *db_color(float db)
{
    if (db >= 95.0f) return RED;
    if (db >= 85.0f) return YELLOW;
    return GREEN;
}

static void render_bar(float score, int width)
{
    int filled = (int)(score / 100.0f * width);
    if (filled > width) filled = width;

    const char *color = score_color(score);

    printf("%s", color);
    for (int i = 0; i < filled; i++)   printf("█");
    printf(RESET DIM);
    for (int i = filled; i < width; i++) printf("░");
    printf(RESET);
}

static void print_dashboard(float db, float score, float time_left, int elapsed)
{
    char time_buf[64];
    format_time(time_left, time_buf, sizeof(time_buf));

    char elapsed_buf[64];
    format_time((float)elapsed, elapsed_buf, sizeof(elapsed_buf));

    printf("\033[H\033[2J");

    printf(BOLD CYAN "  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║          🎧  HEARING STRAIN MONITOR             ║\n");
    printf("  ╠══════════════════════════════════════════════════╣" RESET "\n");

    /* Input mode indicator */
#ifdef RASPBERRY_PI
    printf("  ║  " DIM "Mode:" RESET "        " CYAN "I2S MEMS Microphone" RESET "\n");
#else
    printf("  ║  " DIM "Mode:" RESET "        " DIM "Simulation" RESET "\n");
#endif

    /* dB level */
    printf("  ║  " DIM "dB Level:" RESET "    %s%-6.1f dB" RESET, db_color(db), db);
    if (db >= 95.0f)       printf("   " RED "🔴 VERY LOUD" RESET);
    else if (db >= 85.0f)  printf("   " YELLOW "🟡 LOUD" RESET);
    else if (db >= 70.0f)  printf("   " GREEN "🟢 MODERATE" RESET);
    else                   printf("   " DIM "🔵 QUIET" RESET);
    printf("\n");

    /* Strain bar */
    printf("  ║  " DIM "Strain:" RESET "      ");
    render_bar(score, 25);
    printf("  %s%5.1f" RESET " / 100\n", score_color(score), score);

    /* Time remaining */
    printf("  ║  " DIM "Time Left:" RESET "   %s%-18s" RESET "\n",
           score_color(score), time_buf);

    /* Elapsed time */
    printf("  ║  " DIM "Elapsed:" RESET "     %-18s\n", elapsed_buf);

    printf(BOLD CYAN "  ╠══════════════════════════════════════════════════╣" RESET "\n");

    /* Status line with alert */
    printf("  ║  " DIM "Status:" RESET "  ");
    if (score >= 100.0f) {
        printf(BOLD BG_RED WHITE "  🛑  THRESHOLD REACHED — TAKE A BREAK  " RESET);
    } else if (score >= ALERT_DANGER) {
        printf(BOLD RED "  ⛔  DANGER — HIGH EXPOSURE  " RESET);
    } else if (score >= ALERT_WARNING) {
        printf(BOLD YELLOW "  ⚠   WARNING — MODERATE EXPOSURE  " RESET);
    } else {
        printf(BOLD GREEN "  ✅  SAFE — LOW EXPOSURE  " RESET);
    }
    printf("\n");

    printf(BOLD CYAN "  ╚══════════════════════════════════════════════════╝" RESET "\n");

    printf("\n" DIM "  Press Ctrl-C to stop monitoring." RESET "\n");

    fflush(stdout);
}

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void)
{
    pthread_t audio_tid, proc_tid;

    srand((unsigned)time(NULL));

    /* ── Initialize shared data ──────────────────────────────────── */
    memset(&shared, 0, sizeof(shared));
    shared.running = 1;
    pthread_mutex_init(&shared.lock, NULL);

    /* ── Initialize GPIO (no-op on non-Pi) ───────────────────────── */
    if (gpio_init() != 0) {
        fprintf(stderr, "warning: GPIO init failed, continuing without hardware indicators\n");
    }

    /* ── Install signal handler for graceful shutdown ─────────────── */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    /* ── Spawn worker threads ────────────────────────────────────── */
    if (pthread_create(&audio_tid, NULL, audio_thread_func, &shared) != 0) {
        perror("pthread_create(audio)");
        gpio_shutdown();
        return 1;
    }
    if (pthread_create(&proc_tid, NULL, processing_thread_func, &shared) != 0) {
        perror("pthread_create(processing)");
        gpio_shutdown();
        return 1;
    }

    printf("\033[?25l");  /* hide cursor */

    /* ── Display loop (runs on main thread) ──────────────────────── */
    int elapsed = 0;
    while (1) {
        float db, score, time_left;

        pthread_mutex_lock(&shared.lock);
        if (!shared.running) {
            pthread_mutex_unlock(&shared.lock);
            break;
        }
        db        = shared.db;
        score     = shared.score;
        time_left = shared.time_left;
        pthread_mutex_unlock(&shared.lock);

        print_dashboard(db, score, time_left, elapsed);

        /* ── Drive GPIO indicators ───────────────────────────────── */
        gpio_update(score);

        usleep(DISPLAY_INTERVAL_US);
        elapsed++;
    }

    /* ── Clean shutdown ──────────────────────────────────────────── */
    printf("\033[?25h");   /* restore cursor */

    pthread_join(audio_tid, NULL);
    pthread_join(proc_tid, NULL);
    pthread_mutex_destroy(&shared.lock);

    gpio_shutdown();

    printf("\n" BOLD CYAN "  Hearing Strain Monitor stopped." RESET "\n");
    printf(DIM "  Final strain score: %.1f / 100\n" RESET, shared.score);
    printf(DIM "  Total monitoring time: %d seconds\n" RESET, elapsed);

    return 0;
}
