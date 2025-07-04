# SPDX-FileCopyrightText: Copyright 2010-present Greg Hurrell and contributors.
# SPDX-License-Identifier: BSD-2-Clause

SHELL := /bin/bash

CCFLAGS ?= -std=gnu17 -march=native -mtune=native -Wall -Wextra -Wno-unused-parameter

# Allow augmentation from the command-line with CFLAGS:
CCFLAGS += $(CFLAGS)

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
	# 128 M candidates.
	CCFLAGS += -DMAX_FILES_CONF=134217728
	# 128 GB.
	CCFLAGS += -DMMAP_SLAB_SIZE_CONF=137438953472
else
	# 32 M candidates.
	CCFLAGS += -DMAX_FILES_CONF=33554432
	# 32 GB.
	CCFLAGS += -DMMAP_SLAB_SIZE_CONF=34359738368
endif

ifdef DEBUG
	CCFLAGS += -DDEBUG -g -O0
else
	ifdef PROFILE
		CCFLAGS += -DNDEBUG -g -O3 -ffast-math
	else
		# As per `man 3 assert`, defining `NDEBUG` elides all `assert()` macros.
		CCFLAGS += -DNDEBUG -O3 -ffast-math
	endif
endif

ifeq ($(OS),Windows_NT)
	CC ?= gcc
	CCFLAGS += -DWIN32
	DLLEXT := dll
else
	DLLEXT := so
	UNAME := $(shell uname -s)
	ifeq ($(UNAME),Darwin)
		CC = xcrun clang
		CCFLAGS += -DMACOS
	endif
	ifeq ($(UNAME),Linux)
		CC ?= gcc
		CCFLAGS += -DLINUX -pthread
	endif
endif

LIBS = -lpthread
HDRS = $(wildcard *.h)
SRCS = $(wildcard *.c)

all: commandt.$(DLLEXT)

# Rebuild whenever CCFLAGS change (eg. when DEBUG/PROFILE passed or not passed).
# See: https://stackoverflow.com/a/74378629/2103996
.PHONY: .FORCE
define DEPEND_ON
.make/$1:
	@if [[ `cat .make/$1 2>&1` != "$($1)" ]]; then \
		printf '%s' "$($1)" > .make/$1; \
	fi
ifneq ("$(file <.make/$1)","$($1)")
.make/$1: .FORCE
endif
endef
$(eval $(call DEPEND_ON,CCFLAGS))

commandt.$(DLLEXT): $(HDRS) $(SRCS) Makefile .make/CCFLAGS
	$(CC) $(CCFLAGS) -shared -fPIC -o commandt.$(DLLEXT) $(SRCS)

.PHONY: clean
clean:
	rm -f *.$(DLLEXT)
	rm -f .make/*
	rm -rf *.so.dSYM
