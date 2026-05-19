-include config.mk

CC ?= cc
CXX ?= c++
AR ?= ar
RANLIB ?= ranlib
WERROR ?= 1
WARNFLAGS ?= -Wall -Wextra -Wpedantic
CXXWARNFLAGS ?= -Wall -Wextra
ifeq ($(WERROR),1)
WARNFLAGS += -Werror
CXXWARNFLAGS += -Werror
endif
CFLAGS ?= -std=c99 $(WARNFLAGS) -O2 -g
CXXFLAGS ?= -std=c++11 $(CXXWARNFLAGS) -O2 -g
CPPFLAGS ?= -Iinclude -Isrc
LDFLAGS ?=
ARCH_CFLAGS ?=
ARCH_CXXFLAGS ?= $(ARCH_CFLAGS)
ARCH_LDFLAGS ?=
TEST_ENV ?=
BENCH_FRAMES ?= 20000
PLATFORM_BENCH_FRAMES ?= 2000
CPPCHECK ?= cppcheck
SCAN_BUILD ?= scan-build
ENABLE_SHARED ?= 0
ENABLE_TOOLS ?= 1
ENABLE_TESTS ?= 1
ENABLE_EXAMPLES ?= 1
PICFLAGS ?= -fPIC
SHARED_EXT ?= so
SHARED_LDFLAGS ?= -shared
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 0755
INSTALL_DATA ?= $(INSTALL) -m 0644

CFLAGS += $(ARCH_CFLAGS)
CXXFLAGS += $(ARCH_CXXFLAGS)
LDFLAGS += $(ARCH_LDFLAGS)
ifeq ($(ENABLE_SHARED),1)
CFLAGS += $(PICFLAGS)
endif

BUILD_DIR := build
LIB := $(BUILD_DIR)/libg729.a
SHARED_LIB := $(BUILD_DIR)/libg729.$(SHARED_EXT)
PC_FILE := $(BUILD_DIR)/g729.pc

LIB_OBJS := \
	$(BUILD_DIR)/src/g729.o \
	$(BUILD_DIR)/src/g729_bitstream.o \
	$(BUILD_DIR)/src/g729_closedloop.o \
	$(BUILD_DIR)/src/g729_fcb.o \
	$(BUILD_DIR)/src/g729_fcb_search.o \
	$(BUILD_DIR)/src/g729_fixed.o \
	$(BUILD_DIR)/src/g729_gain.o \
	$(BUILD_DIR)/src/g729_gain_quant.o \
	$(BUILD_DIR)/src/g729_gain_tables.o \
	$(BUILD_DIR)/src/g729_hp.o \
	$(BUILD_DIR)/src/g729_lpc.o \
	$(BUILD_DIR)/src/g729_lsp.o \
	$(BUILD_DIR)/src/g729_pcm.o \
	$(BUILD_DIR)/src/g729_openloop.o \
	$(BUILD_DIR)/src/g729_pitch.o \
	$(BUILD_DIR)/src/g729_postfilter.o \
	$(BUILD_DIR)/src/g729_synth.o \
	$(BUILD_DIR)/src/g729_lsp_tables.o

TEST_BINS := \
	$(BUILD_DIR)/tests/test_api \
	$(BUILD_DIR)/tests/test_bitstream \
	$(BUILD_DIR)/tests/test_cli_decode \
	$(BUILD_DIR)/tests/test_cli_encode \
	$(BUILD_DIR)/tests/test_closedloop \
	$(BUILD_DIR)/tests/test_decode \
	$(BUILD_DIR)/tests/test_encode \
	$(BUILD_DIR)/tests/test_fcb \
	$(BUILD_DIR)/tests/test_fcb_search \
	$(BUILD_DIR)/tests/test_fixed \
	$(BUILD_DIR)/tests/test_gain \
	$(BUILD_DIR)/tests/test_gain_quant \
	$(BUILD_DIR)/tests/test_hp \
	$(BUILD_DIR)/tests/test_lpc \
	$(BUILD_DIR)/tests/test_loopback \
	$(BUILD_DIR)/tests/test_lsp \
	$(BUILD_DIR)/tests/test_lsp_encode \
	$(BUILD_DIR)/tests/test_openloop \
	$(BUILD_DIR)/tests/test_pcm \
	$(BUILD_DIR)/tests/test_pitch \
	$(BUILD_DIR)/tests/test_postfilter \
	$(BUILD_DIR)/tests/test_stress \
	$(BUILD_DIR)/tests/test_synth

EXAMPLE_BINS := \
	$(BUILD_DIR)/examples/api_smoke \
	$(BUILD_DIR)/examples/cpp_smoke

TOOL_BINS := \
	$(BUILD_DIR)/tools/g729bench \
	$(BUILD_DIR)/tools/g729enc \
	$(BUILD_DIR)/tools/g729dec

.PHONY: all clean distclean install uninstall test fixtures closedloop-oracle decode-oracle encode-oracle fcb-oracle fcb-search-oracle gain-oracle gain-quant-oracle gain-tables hp-oracle loadtest lpc-oracle lsp-enc-oracle lsp-tables lsp-oracle openloop-oracle pcm-oracle pitch-oracle platform-check postfilter-oracle release-check static-analysis synth-oracle sanitize

LIB_TARGETS := $(LIB)
ifeq ($(ENABLE_SHARED),1)
LIB_TARGETS += $(SHARED_LIB)
endif

ALL_TARGETS := $(LIB_TARGETS)
ifeq ($(ENABLE_TOOLS),1)
ALL_TARGETS += $(TOOL_BINS)
endif
ifeq ($(ENABLE_TESTS),1)
ALL_TARGETS += $(TEST_BINS)
endif
ifeq ($(ENABLE_EXAMPLES),1)
ALL_TARGETS += $(EXAMPLE_BINS)
endif

INSTALL_TARGETS := $(LIB_TARGETS) $(PC_FILE)
ifeq ($(ENABLE_TOOLS),1)
INSTALL_TARGETS += $(TOOL_BINS)
endif

all: $(ALL_TARGETS)

$(BUILD_DIR)/src/%.o: src/%.c include/g729.h src/g729_bitstream.h src/g729_closedloop.h src/g729_fcb.h src/g729_fcb_search.h src/g729_fixed.h src/g729_gain.h src/g729_gain_quant.h src/g729_gain_tables.h src/g729_hp.h src/g729_lpc.h src/g729_lsp.h src/g729_lsp_tables.h src/g729_openloop.h src/g729_pcm.h src/g729_pitch.h src/g729_postfilter.h src/g729_synth.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(SHARED_LIB): $(LIB_OBJS)
	@mkdir -p $(@D)
	$(CC) $(SHARED_LDFLAGS) $(LDFLAGS) -o $@ $^

$(PC_FILE):
	@mkdir -p $(@D)
	@printf '%s\n' \
		'prefix=$(PREFIX)' \
		'exec_prefix=$${prefix}' \
		'libdir=$(LIBDIR)' \
		'includedir=$(INCLUDEDIR)' \
		'' \
		'Name: g729-c' \
		'Description: Clean-room G.729 Annex A-compatible C codec library' \
		'Version: 0.1.0' \
		'Libs: -L$${libdir} -lg729' \
		'Cflags: -I$${includedir}' > $@

$(BUILD_DIR)/tests/%: tests/%.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/tests/test_cli_decode: tests/test_cli_decode.c tests/fixtures/decode_oracle_vectors.h $(LIB) $(BUILD_DIR)/tools/g729dec
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-DG729DEC_BIN=\"$(abspath $(BUILD_DIR)/tools/g729dec)\" \
		-DG729_CLI_TEST_DIR=\"$(abspath $(BUILD_DIR)/tests)\" \
		$< $(LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/tests/test_cli_encode: tests/test_cli_encode.c tests/fixtures/encode_oracle_vectors.h $(LIB) $(BUILD_DIR)/tools/g729enc
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-DG729ENC_BIN=\"$(abspath $(BUILD_DIR)/tools/g729enc)\" \
		-DG729_CLI_TEST_DIR=\"$(abspath $(BUILD_DIR)/tests)\" \
		$< $(LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/tests/test_pitch: tests/fixtures/pitch_oracle_vectors.h
$(BUILD_DIR)/tests/test_synth: tests/fixtures/synth_oracle_vectors.h
$(BUILD_DIR)/tests/test_gain: tests/fixtures/gain_oracle_vectors.h
$(BUILD_DIR)/tests/test_gain_quant: tests/fixtures/gain_quant_oracle_vectors.h
$(BUILD_DIR)/tests/test_closedloop: tests/fixtures/closedloop_oracle_vectors.h
$(BUILD_DIR)/tests/test_lsp: tests/fixtures/lsp_oracle_vectors.h
$(BUILD_DIR)/tests/test_lsp_encode: tests/fixtures/lsp_encode_oracle_vectors.h
$(BUILD_DIR)/tests/test_openloop: tests/fixtures/openloop_oracle_vectors.h
$(BUILD_DIR)/tests/test_lpc: tests/fixtures/lpc_oracle_vectors.h
$(BUILD_DIR)/tests/test_loopback: tests/fixtures/encode_oracle_vectors.h tests/fixtures/decode_oracle_vectors.h
$(BUILD_DIR)/tests/test_fcb: tests/fixtures/fcb_oracle_vectors.h
$(BUILD_DIR)/tests/test_fcb_search: tests/fixtures/fcb_search_oracle_vectors.h
$(BUILD_DIR)/tests/test_hp: tests/fixtures/hp_oracle_vectors.h
$(BUILD_DIR)/tests/test_postfilter: tests/fixtures/postfilter_oracle_vectors.h
$(BUILD_DIR)/tests/test_decode: tests/fixtures/decode_oracle_vectors.h
$(BUILD_DIR)/tests/test_encode: tests/fixtures/encode_oracle_vectors.h
$(BUILD_DIR)/tests/test_pcm: tests/fixtures/pcm_oracle_vectors.h

$(BUILD_DIR)/examples/%: examples/%.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/examples/cpp_smoke: examples/cpp_smoke.cpp $(LIB)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LIB) $(LDFLAGS) -o $@

$(BUILD_DIR)/tools/%: tools/%.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDFLAGS) -o $@

test: $(TEST_BINS) $(EXAMPLE_BINS) $(TOOL_BINS)
	$(TEST_ENV) $(BUILD_DIR)/tests/test_api
	$(TEST_ENV) $(BUILD_DIR)/tests/test_bitstream
	$(TEST_ENV) $(BUILD_DIR)/tests/test_cli_decode
	$(TEST_ENV) $(BUILD_DIR)/tests/test_cli_encode
	$(TEST_ENV) $(BUILD_DIR)/tests/test_closedloop
	$(TEST_ENV) $(BUILD_DIR)/tests/test_decode
	$(TEST_ENV) $(BUILD_DIR)/tests/test_encode
	$(TEST_ENV) $(BUILD_DIR)/tests/test_fcb
	$(TEST_ENV) $(BUILD_DIR)/tests/test_fcb_search
	$(TEST_ENV) $(BUILD_DIR)/tests/test_fixed
	$(TEST_ENV) $(BUILD_DIR)/tests/test_gain
	$(TEST_ENV) $(BUILD_DIR)/tests/test_gain_quant
	$(TEST_ENV) $(BUILD_DIR)/tests/test_hp
	$(TEST_ENV) $(BUILD_DIR)/tests/test_lpc
	$(TEST_ENV) $(BUILD_DIR)/tests/test_loopback
	$(TEST_ENV) $(BUILD_DIR)/tests/test_lsp
	$(TEST_ENV) $(BUILD_DIR)/tests/test_lsp_encode
	$(TEST_ENV) $(BUILD_DIR)/tests/test_openloop
	$(TEST_ENV) $(BUILD_DIR)/tests/test_pcm
	$(TEST_ENV) $(BUILD_DIR)/tests/test_pitch
	$(TEST_ENV) $(BUILD_DIR)/tests/test_postfilter
	$(TEST_ENV) $(BUILD_DIR)/tests/test_stress
	$(TEST_ENV) $(BUILD_DIR)/tests/test_synth
	$(TEST_ENV) $(BUILD_DIR)/examples/api_smoke
	$(TEST_ENV) $(BUILD_DIR)/examples/cpp_smoke

loadtest: $(BUILD_DIR)/tools/g729bench
	$(BUILD_DIR)/tools/g729bench $(BENCH_FRAMES)

install: $(INSTALL_TARGETS)
	$(INSTALL) -d "$(DESTDIR)$(INCLUDEDIR)"
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)"
	$(INSTALL) -d "$(DESTDIR)$(PKGCONFIGDIR)"
	$(INSTALL_DATA) include/g729.h "$(DESTDIR)$(INCLUDEDIR)/g729.h"
	$(INSTALL_DATA) $(LIB) "$(DESTDIR)$(LIBDIR)/libg729.a"
ifeq ($(ENABLE_SHARED),1)
	$(INSTALL_PROGRAM) $(SHARED_LIB) "$(DESTDIR)$(LIBDIR)/libg729.$(SHARED_EXT)"
endif
	$(INSTALL_DATA) $(PC_FILE) "$(DESTDIR)$(PKGCONFIGDIR)/g729.pc"
ifeq ($(ENABLE_TOOLS),1)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL_PROGRAM) $(BUILD_DIR)/tools/g729enc "$(DESTDIR)$(BINDIR)/g729enc"
	$(INSTALL_PROGRAM) $(BUILD_DIR)/tools/g729dec "$(DESTDIR)$(BINDIR)/g729dec"
	$(INSTALL_PROGRAM) $(BUILD_DIR)/tools/g729bench "$(DESTDIR)$(BINDIR)/g729bench"
endif

uninstall:
	rm -f "$(DESTDIR)$(INCLUDEDIR)/g729.h"
	rm -f "$(DESTDIR)$(LIBDIR)/libg729.a"
	rm -f "$(DESTDIR)$(LIBDIR)/libg729.$(SHARED_EXT)"
	rm -f "$(DESTDIR)$(PKGCONFIGDIR)/g729.pc"
	rm -f "$(DESTDIR)$(BINDIR)/g729enc"
	rm -f "$(DESTDIR)$(BINDIR)/g729dec"
	rm -f "$(DESTDIR)$(BINDIR)/g729bench"

platform-check:
	$(MAKE) clean test
	$(MAKE) loadtest BENCH_FRAMES=$(PLATFORM_BENCH_FRAMES)

release-check:
	$(MAKE) clean test
	$(MAKE) sanitize
	$(MAKE) clean test
	$(MAKE) loadtest
	git diff --check

static-analysis:
	$(MAKE) clean test CC=clang CXX=clang++
	$(SCAN_BUILD) --status-bugs -o /tmp/g729-c-scan-build \
		$(MAKE) clean test CC=clang CXX=clang++
	$(CPPCHECK) --std=c99 --enable=warning,performance,portability \
		--error-exitcode=1 --inline-suppr --suppress=missingIncludeSystem \
		$(CPPFLAGS) src tests tools examples/api_smoke.c
	$(CPPCHECK) --std=c++11 --enable=warning,performance,portability \
		--error-exitcode=1 --inline-suppr --suppress=missingIncludeSystem \
		$(CPPFLAGS) examples/cpp_smoke.cpp

fixtures:
	cd tools/oracle-gen && GOCACHE=/tmp/go-build go run . -out ../../testdata/oracle/basic_vectors.json

closedloop-oracle:
	cd tools/closedloop-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/closedloop_oracle_vectors.h

decode-oracle:
	cd tools/decode-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/decode_oracle_vectors.h

encode-oracle:
	cd tools/encode-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/encode_oracle_vectors.h

fcb-oracle:
	cd tools/fcb-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/fcb_oracle_vectors.h

fcb-search-oracle:
	cd tools/fcb-search-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/fcb_search_oracle_vectors.h

gain-oracle:
	cd tools/gain-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/gain_oracle_vectors.h

gain-quant-oracle:
	cd tools/gain-quant-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/gain_quant_oracle_vectors.h

gain-tables:
	cd tools/port-gain-tables && GOCACHE=/tmp/go-build go run . -out ../../src/g729_gain_tables.c

hp-oracle:
	cd tools/hp-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/hp_oracle_vectors.h

lpc-oracle:
	cd tools/lpc-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/lpc_oracle_vectors.h

lsp-tables:
	cd tools/port-lsp-tables && GOCACHE=/tmp/go-build go run . -out ../../src/g729_lsp_tables.c

lsp-oracle:
	cd tools/lsp-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/lsp_oracle_vectors.h

lsp-enc-oracle:
	cd tools/lsp-enc-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/lsp_encode_oracle_vectors.h

openloop-oracle:
	cd tools/openloop-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/openloop_oracle_vectors.h

pcm-oracle:
	cd tools/pcm-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/pcm_oracle_vectors.h

pitch-oracle:
	cd tools/pitch-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/pitch_oracle_vectors.h

postfilter-oracle:
	cd tools/postfilter-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/postfilter_oracle_vectors.h

synth-oracle:
	cd tools/synth-oracle && GOCACHE=/tmp/go-build go run . -out ../../tests/fixtures/synth_oracle_vectors.h

sanitize: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -O1
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: TEST_ENV := ASAN_OPTIONS=detect_leaks=0
sanitize: clean test

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -f config.mk
