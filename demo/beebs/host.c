#include "beebs_shared.h"
#include "penglai-enclave.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BEEBS_ATTEST_NONCE 12345UL

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

/**
 * Redirect standard output to /dev/null in a child process during warm-up runs.
 *
 * Returns true on success, false if /dev/null cannot be opened or duplicated.
 */
static bool silence_stdout(void) {
  int fd = open("/dev/null", O_WRONLY);
  if (fd < 0) {
    return false;
  }
  bool ok = (dup2(fd, STDOUT_FILENO) >= 0);
  close(fd);
  return ok;
}

/**
 * Execute the plain Linux benchmark ELF as a child process.
 *
 * Parameters:
 *   plain_bin: Path to the statically linked plain Linux benchmark binary.
 *   quiet: If true, suppress child stdout (used for cache warm-up).
 * Returns true if the child exits with status 0, false otherwise.
 */
static bool run_plain_process(const char *plain_bin, bool quiet) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid == 0) {
    if (quiet && !silence_stdout()) {
      _exit(126);
    }
    char *const args[] = {(char *)plain_bin, NULL};
    execv(plain_bin, args);
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/**
 * Run a single Penglai-sPMP enclave lifecycle (create, attest, run, finalize).
 *
 * Parameters:
 *   eapp: Path to the Penglai enclave ELF binary.
 *   measure: If true, emit lifecycle and workload cycle counts.
 * Returns true if enclave creation, attestation, execution, verification, and
 * teardown succeed; false otherwise.
 */
static bool run_enclave_lifecycle(const char *eapp, bool measure) {
  struct PLenclave *enclave = calloc(1, sizeof(struct PLenclave));
  struct enclave_args *params = calloc(1, sizeof(struct enclave_args));
  struct elf_args *enclave_file = calloc(1, sizeof(struct elf_args));
  struct beebs_eapp_result res;
  memset(&res, 0, sizeof(res));

  if (!enclave || !params || !enclave_file) {
    printf("[FAIL] %s: host allocation failed\n", eapp);
    free(enclave);
    free(params);
    free(enclave_file);
    return false;
  }

  uint64_t create_start = 0;
  uint64_t create_end = 0;
  uint64_t attest_start = 0;
  uint64_t attest_end = 0;
  uint64_t run_start = 0;
  uint64_t teardown_end = 0;
  bool ok = false;

  if (measure) {
    create_start = read_cycles();
  }

  elf_args_init(enclave_file, (char *)eapp);
  if (!elf_valid(enclave_file)) {
    printf("[FAIL] %s: invalid enclave ELF\n", eapp);
    goto cleanup_elf;
  }

  PLenclave_init(enclave);
  enclave_param_init(params);
  params->untrusted_mem_size = DEFAULT_UNTRUSTED_SIZE;
  params->untrusted_mem_ptr = 0;

  if (PLenclave_create(enclave, enclave_file, params) < 0) {
    printf("[FAIL] %s: PLenclave_create failed\n", eapp);
    goto cleanup_enclave;
  }

  if (measure) {
    create_end = read_cycles();
    attest_start = read_cycles();
  }

  if (PLenclave_attest(enclave, BEEBS_ATTEST_NONCE) < 0) {
    printf("[FAIL] %s: PLenclave_attest failed\n", eapp);
    PLenclave_destroy(enclave);
    goto cleanup_enclave;
  }

  if (measure) {
    attest_end = read_cycles();
  }

  /* Supply user buffer for penglai_enclave_run's copy_to_user on enclave exit. */
  enclave->user_param.untrusted_mem_ptr = (unsigned long)&res;
  enclave->user_param.untrusted_mem_size = (long)sizeof(res);

  if (measure) {
    run_start = read_cycles();
  }

  int run_rc = PLenclave_run(enclave);
  PLenclave_finalize(enclave);
  enclave_param_destroy(params);
  elf_args_destroy(enclave_file);

  if (measure) {
    teardown_end = read_cycles();
  }

  if (run_rc < 0) {
    printf("[FAIL] %s: PLenclave_run failed (%d)\n", eapp, run_rc);
    goto out;
  }

  if (res.magic != BEEBS_RESULT_MAGIC || res.ok != 1ULL) {
    printf("[FAIL] %s: enclave verification failed (magic=0x%lx ok=%lu)\n",
           eapp, (unsigned long)res.magic, (unsigned long)res.ok);
    goto out;
  }

  if (measure) {
    uint64_t create_cycles = create_end - create_start;
    uint64_t attest_cycles = attest_end - attest_start;
    uint64_t boot_cycles = res.start_cycles - run_start;
    uint64_t bench_cycles = res.end_cycles - res.start_cycles;
    uint64_t teardown_cycles = teardown_end - res.exit_start_cycles;
    printf("[PENGLAI-SPMP] %s create_cycles=%lu attest_cycles=%lu "
           "run_start_cycles=%lu start_cycles=%lu end_cycles=%lu "
           "boot_cycles=%lu bench_cycles=%lu exit_start_cycles=%lu "
           "teardown_end_cycles=%lu teardown_cycles=%lu\n",
           eapp,
           (unsigned long)create_cycles,
           (unsigned long)attest_cycles,
           (unsigned long)run_start,
           (unsigned long)res.start_cycles,
           (unsigned long)res.end_cycles,
           (unsigned long)boot_cycles,
           (unsigned long)bench_cycles,
           (unsigned long)res.exit_start_cycles,
           (unsigned long)teardown_end,
           (unsigned long)teardown_cycles);
    fflush(stdout);
  }

  ok = true;
  goto out;

cleanup_enclave:
  PLenclave_finalize(enclave);
  enclave_param_destroy(params);
cleanup_elf:
  elf_args_destroy(enclave_file);
out:
  free(enclave);
  free(params);
  free(enclave_file);
  return ok;
}

/**
 * Execute an untimed enclave warm-up run in a child process.
 */
static bool run_enclave_warmup(const char *eapp) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid == 0) {
    if (!silence_stdout()) {
      _exit(126);
    }
    bool ok = run_enclave_lifecycle(eapp, false);
    _exit(ok ? 0 : 1);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: %s <eapp> <plain-bin>\n", argv[0]);
    return 1;
  }

  const char *eapp = argv[1];
  const char *plain_bin = argv[2];

  if (!run_plain_process(plain_bin, true)) {
    printf("[FAIL] %s: plain warm-up failed\n", eapp);
    return 1;
  }

  printf("[MODE] PLAIN %s\n", eapp);
  fflush(stdout);
  if (!run_plain_process(plain_bin, false)) {
    printf("[FAIL] %s: plain timed run failed\n", eapp);
    return 1;
  }

  if (!run_enclave_warmup(eapp)) {
    printf("[FAIL] %s: enclave warm-up failed\n", eapp);
    return 1;
  }

  printf("[MODE] EAPP %s\n", eapp);
  fflush(stdout);
  if (!run_enclave_lifecycle(eapp, true)) {
    return 1;
  }

  printf("[PASS] %s\n", eapp);
  fflush(stdout);
  return 0;
}
