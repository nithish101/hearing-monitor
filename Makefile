# ──────────────────────────────────────────────────────────────────────
#  Hearing Strain Monitor — Makefile
# ──────────────────────────────────────────────────────────────────────
#  Usage:
#    make            Build the monitor binary
#    make clean      Remove build artifacts
#    make run        Build and run
# ──────────────────────────────────────────────────────────────────────

CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -O2
LDFLAGS  = -lpthread

TARGET   = hearing_monitor
SRCS     = main.c audio.c processing.c
OBJS     = $(SRCS:.c=.o)

# ── Default target ──────────────────────────────────────────────────

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c shared.h audio.h processing.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Convenience targets ────────────────────────────────────────────

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean run
