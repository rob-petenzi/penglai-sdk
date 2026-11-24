CC ?= riscv64-unknown-linux-gnu-gcc
LINK ?= riscv64-unknown-linux-gnu-ld
AR ?= riscv64-unknown-linux-gnu-ar
ARCH_FLAGS ?= -march=rv64imafd -mabi=lp64d
BEEBS_DIR ?= $(CURDIR)/../../../../beebs

SDK_LIB_DIR := $(PENGLAI_SDK)/lib
MUSL_LIB_DIR := $(PENGLAI_SDK)/musl/lib
MUSL_LIBC := $(MUSL_LIB_DIR)/libc.a
SDK_APP_LIB := $(SDK_LIB_DIR)/libpenglai-enclave-eapp.a
SDK_HOST_LIB := $(SDK_LIB_DIR)/libpenglai-enclave-host.a
METADATA_OBJ := $(SDK_LIB_DIR)/app/src/metadata_sec.o
GCC_LIB := $(shell $(CC) $(ARCH_FLAGS) -print-libgcc-file-name)
APP_LDS := $(PENGLAI_SDK)/app.lds

BENCH_EAPP_CFLAGS := $(EAPP_CFLAGS) -Os -g0 -DNDEBUG -I$(BEEBS_DIR)/include -I$(CURDIR)
BENCH_PLAIN_CFLAGS := -Os -g0 -DNDEBUG $(ARCH_FLAGS) -I$(BEEBS_DIR)/include -Wall
HOST_CFLAGS := -O2 -Wall -I$(SDK_LIB_DIR)/host/include -I$(CURDIR)

BENCHMARKS := \
	bubblesort:bubblesort/bubblesort.c \
	compress:compress/compress.c \
	dijkstra:dijkstra/dijkstra_small.c \
	insertsort:insertsort/insertsort.c \
	nsichneu:nsichneu/nsichneu.c \
	qsort:qsort/qsort.c \
	statemate:statemate/statemate.c \
	tarai:tarai/tarai.c

EAPP_BINS := $(foreach entry,$(BENCHMARKS),beebs-$(firstword $(subst :, ,$(entry))))
PLAIN_BINS := $(foreach entry,$(BENCHMARKS),beebs-$(firstword $(subst :, ,$(entry)))-plain)

.PHONY: all clean

all: beebs-runner $(EAPP_BINS) $(PLAIN_BINS)

beebs-runner: host.c beebs_shared.h $(SDK_HOST_LIB)
	$(CC) $(HOST_CFLAGS) -static -o $@ host.c $(SDK_HOST_LIB) -lpthread

beebs_main_eapp.o: $(BEEBS_DIR)/src/main.c
	$(CC) $(BENCH_EAPP_CFLAGS) -Dmain=beebs_main -c -o $@ $<

support_stubs_eapp.o: support_stubs_eapp.c beebs_shared.h
	$(CC) $(BENCH_EAPP_CFLAGS) -c -o $@ $<

beebs_main_plain.o: $(BEEBS_DIR)/src/main.c
	$(CC) $(BENCH_PLAIN_CFLAGS) -c -o $@ $<

support_stubs_plain.o: support_stubs_plain.c
	$(CC) $(BENCH_PLAIN_CFLAGS) -c -o $@ $<

define BENCH_RULES
beebs-$(1).o: $(BEEBS_DIR)/src/$(2)
	$$(CC) $$(BENCH_EAPP_CFLAGS) -c -o $$@ $$<

beebs-$(1): support_stubs_eapp.o beebs_main_eapp.o beebs-$(1).o $$(SDK_APP_LIB) $$(MUSL_LIBC) $$(GCC_LIB) $$(METADATA_OBJ)
	$$(LINK) -static -L$$(SDK_LIB_DIR) -L$$(MUSL_LIB_DIR) -o $$@ \
		support_stubs_eapp.o beebs_main_eapp.o beebs-$(1).o \
		--start-group $$(SDK_APP_LIB) $$(MUSL_LIBC) $$(GCC_LIB) --end-group \
		$$(METADATA_OBJ) -T $$(APP_LDS)
	chmod -x $$@

beebs-$(1)-plain.o: $(BEEBS_DIR)/src/$(2)
	$$(CC) $$(BENCH_PLAIN_CFLAGS) -c -o $$@ $$<

beebs-$(1)-plain: beebs_main_plain.o support_stubs_plain.o beebs-$(1)-plain.o
	$$(CC) $$(BENCH_PLAIN_CFLAGS) -static -o $$@ $$^
endef

$(foreach entry,$(BENCHMARKS),$(eval $(call BENCH_RULES,$(firstword $(subst :, ,$(entry))),$(lastword $(subst :, ,$(entry))))))

clean:
	rm -f *.o beebs-runner $(EAPP_BINS) $(PLAIN_BINS)
