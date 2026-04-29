CLANG ?= clang
CC ?= cc
BPFTOOL ?= /usr/sbin/bpftool

ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')
BPF_SYS_INCLUDES := -I/usr/include -I/usr/include/$(shell uname -m)-linux-gnu
BPF_CFLAGS := -target bpf -D__TARGET_ARCH_$(ARCH) -O2 -g -Wall -Werror
USER_CFLAGS := -O2 -g -Wall -Wextra -Werror
USER_LDLIBS := $(shell pkg-config --libs libbpf libelf zlib)
USER_CPPFLAGS := -Isrc $(shell pkg-config --cflags libbpf)
EXERCISE_LDFLAGS := -static

.DELETE_ON_ERROR:

.PHONY: all clean

all: lbr_snapshot exercise filler

src/lbr_snapshot.skel.h: src/lbr_snapshot.bpf.o
	$(BPFTOOL) gen skeleton $< > $@.tmp
	mv $@.tmp $@

src/lbr_snapshot.bpf.o: src/lbr_snapshot.bpf.c src/common.h
	$(CLANG) $(BPF_CFLAGS) $(BPF_SYS_INCLUDES) -Isrc -c $< -o $@

lbr_snapshot: src/lbr_snapshot.c src/lbr_snapshot.skel.h src/common.h
	$(CC) $(USER_CFLAGS) $(USER_CPPFLAGS) $< $(USER_LDLIBS) -o $@

exercise: src/exercise.c
	@# Note: this must be linked statically to generate addresses in LBR that are easier to interpret:
	@#       all addresses will refer to the executable image (no shared libraries with dedicated mappings).
	$(CC) $(USER_CFLAGS) $< $(EXERCISE_LDFLAGS) -o $@

filler: src/filler.c
	@# Note: don't compile with optimizations, because we just need to fill the LBR with some entries.
	@#       The code is currently pretty much a no-op, so the compiler might optimize it away.
	$(CC) $(USER_CFLAGS) $< -o $@

clean:
	rm -f lbr_snapshot exercise filler src/lbr_snapshot.bpf.o src/lbr_snapshot.skel.h