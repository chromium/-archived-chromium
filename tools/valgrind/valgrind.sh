#!/bin/sh
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a small script for manually launching valgrind, along with passing
# it the suppression file, and some helpful arguments (automatically attaching
# the debugger on failures, etc).  Run it from your repo root, something like:
#  $ sh ./tools/valgrind/valgrind.sh ./sconsbuild/Debug/chrome
#
# This is mostly intended for running the chrome browser interactively.
# To run unit tests, you probably want to run chrome_tests.sh instead.
# That's the script used by the valgrind buildbot.

set -e

if [ $# -eq 0 ]; then
  echo "usage: <command to run> <arguments ...>"
  exit 1
fi

# Prefer a 32-bit gdb if it's available.
GDB="/usr/bin/gdb32";
if [ ! -x $GDB ]; then
  GDB="gdb"
fi

# Prefer a local install of valgrind if it's available.
VALGRIND="/usr/local/bin/valgrind"
if [ ! -x $VALGRIND ]; then
  VALGRIND="valgrind"
fi

SUPPRESSIONS="$(cd `dirname "$0"` && pwd)/suppressions.txt"

set -v

# Pass GTK glib allocations through to system malloc so valgrind sees them.
# Prevent NSS from recycling memory arenas so valgrind can track origins.
# Ask GTK to abort on any critical or warning assertions.
# Overwrite newly allocated or freed objects with 0x41 to catch inproper use.
# smc-check=all is required for valgrind to see v8's dynamic code generation.
# trace-children to follow into the renderer processes.
# Prompt to attach gdb when there was an error detected.
G_SLICE=always-malloc \
NSS_DISABLE_ARENA_FREE_LIST=1 \
G_DEBUG=fatal_warnings \
"$VALGRIND" \
  --trace-children=yes \
  --db-command="$GDB -nw %f %p" \
  --db-attach=yes \
  --suppressions="$SUPPRESSIONS" \
  --malloc-fill=41 --free-fill=41 \
  --smc-check=all \
  "$@"
