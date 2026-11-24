#ifndef _PENGLAI_BEEBS_SHARED_H
#define _PENGLAI_BEEBS_SHARED_H

#include <stdint.h>

#define BEEBS_RESULT_MAGIC 0x4245454253524553ULL

struct beebs_eapp_result {
  uint64_t magic;
  uint64_t ok;
  uint64_t start_cycles;
  uint64_t end_cycles;
  uint64_t exit_start_cycles;
};

#endif
