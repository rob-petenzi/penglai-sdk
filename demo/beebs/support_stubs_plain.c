#include <stdint.h>
#include <stdio.h>
#include "support.h"

static volatile uint64_t timing_guard_lo[8];
static uint64_t start_cycles;
static uint64_t end_cycles;
static volatile uint64_t timing_guard_hi[8];

/**
 * Read the RISC-V hardware cycle counter via rdcycle.
 *
 * Preconditions: Counter access (CY bit) must be enabled in mcounteren and
 * scounteren for U-mode execution.
 * Returns the 64-bit cycle count.
 */
static inline uint64_t read_cycles(void) {
  uint64_t cycles;
  asm volatile("rdcycle %0" : "=r"(cycles));
  return cycles;
}

void initialise_board(void) {}

void start_trigger(void) {
  start_cycles = read_cycles();
}

void stop_trigger(void) {
  end_cycles = read_cycles();
  printf("[BEEBS] start_cycles=%lu end_cycles=%lu bench_cycles=%lu\n",
         (unsigned long)start_cycles,
         (unsigned long)end_cycles,
         (unsigned long)(end_cycles - start_cycles));
  fflush(stdout);
}
