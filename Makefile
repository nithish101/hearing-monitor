# ──────────────────────────────────────────────────────────────────────
#  Hearing Strain Monitor — Makefile
# ──────────────────────────────────────────────────────────────────────
#  Usage:
#    make            Build in simulation mode (macOS / Linux desktop)
#    make pi         Build for Raspberry Pi with ALSA + GPIO
#    make clean      Remove build artifacts
#    make run        Build (simulation) and run
# ──────────────────────────────────────────────────────────────────────

CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -O2
LDFLAGS  = -lpthread

TARGET   = hearing_monitor
SRCS     = main.c audio.c processing.c gpio.c
OBJS     = $(SRCS:.c=.o)

# ── Default target (simulation mode) ───────────────────────────────

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c shared.h audio.h processing.h gpio.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Raspberry Pi target ───────────────────────────────────────────

pi: CFLAGS  += -DRASPBERRY_PI
pi: LDFLAGS += -lasound -lpigpio -lm
pi: clean $(TARGET)

# ── Convenience targets ───────────────────────────────────────────

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean run pi
