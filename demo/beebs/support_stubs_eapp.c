#include <stdint.h>
#include "beebs_shared.h"
#include "eapp.h"
#include "print.h"
#include "support.h"

extern int beebs_main(void);

/* Guard padding isolates timing state from upstream qsort.c's 1-indexed arr[20] write. */
static volatile uint64_t g_timing_guard_lo[8];
static uint64_t g_start_cycles;
static uint64_t g_end_cycles;
static volatile uint64_t g_timing_guard_hi[8];

/**
 * Read the RISC-V hardware cycle counter via rdcycle.
 *
 * Preconditions: Counter access (CY bit) must be enabled in mcounteren and
 * scounteren for enclave U-mode execution.
 * Returns the 64-bit cycle count.
 */
static inline uint64_t read_cycles(void) {
  uint64_t cycles;
  asm volatile("rdcycle %0" : "=r"(cycles));
  return cycles;
}

void initialise_board(void) {}

void start_trigger(void) {
  g_start_cycles = read_cycles();
}

void stop_trigger(void) {
  g_end_cycles = read_cycles();
  eapp_print("[BEEBS] start_cycles=%ld end_cycles=%ld bench_cycles=%ld\n",
             (long)g_start_cycles,
             (long)g_end_cycles,
             (long)(g_end_cycles - g_start_cycles));
}

static void __attribute__((noinline, noreturn))
beebs_eapp_run(unsigned long *args) {
  int rc = beebs_main();
  volatile struct beebs_eapp_result *res =
      (volatile struct beebs_eapp_result *)get_untrusted_mem_ptr(args);
  if (res) {
    res->ok = (rc == 0) ? 1ULL : 0ULL;
    res->start_cycles = g_start_cycles;
    res->end_cycles = g_end_cycles;
    res->magic = BEEBS_RESULT_MAGIC;
    /* Sample immediately before EAPP_RETURN so teardown excludes ocall I/O. */
    res->exit_start_cycles = read_cycles();
  }
  EAPP_RETURN(0);
}

int EAPP_ENTRY _eapp_entry(void) {
  unsigned long *args;
  EAPP_RESERVE_REG;
  beebs_eapp_run(args);
}
