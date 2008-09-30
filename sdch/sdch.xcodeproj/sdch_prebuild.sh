#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"

# Generate config.h the same way that "sh configure" would
mkdir -p "${GENERATED_DIR}"

CONFIG_H_IN="$1"
if test "X${CONFIG_H_IN}" = "X"; then
  CONFIG_H_IN="open-vcdiff/src/config.h.in"
fi

CONFIG_H="${GENERATED_DIR}/config.h"
sed -e s/'^#undef HAVE_DLFCN_H$'/'#define HAVE_DLFCN_H 1'/ \
    -e s/'^#undef HAVE_FNMATCH_H$'/'#define HAVE_FNMATCH_H 1'/ \
    -e s/'^#undef HAVE_GETOPT_H$'/'#define HAVE_GETOPT_H 1'/ \
    -e s/'^#undef HAVE_GETTIMEOFDAY$'/'#define HAVE_GETTIMEOFDAY 1'/ \
    -e s/'^#undef HAVE_INTTYPES_H$'/'#define HAVE_INTTYPES_H 1'/ \
    -e s/'^#undef HAVE_MEMORY_H$'/'#define HAVE_MEMORY_H 1'/ \
    -e s/'^#undef HAVE_MPROTECT$'/'#define HAVE_MPROTECT 1'/ \
    -e s/'^#undef HAVE_PTHREAD$'/'#define HAVE_PTHREAD 1'/ \
    -e s/'^#undef HAVE_STDINT_H$'/'#define HAVE_STDINT_H 1'/ \
    -e s/'^#undef HAVE_STDLIB_H$'/'#define HAVE_STDLIB_H 1'/ \
    -e s/'^#undef HAVE_STRINGS_H$'/'#define HAVE_STRINGS_H 1'/ \
    -e s/'^#undef HAVE_STRING_H$'/'#define HAVE_STRING_H 1'/ \
    -e s/'^#undef HAVE_STRTOLL$'/'#define HAVE_STRTOLL 1'/ \
    -e s/'^#undef HAVE_STRTOQ$'/'#define HAVE_STRTOQ 1'/ \
    -e s/'^#undef HAVE_SYS_MMAN_H$'/'#define HAVE_SYS_MMAN_H 1'/ \
    -e s/'^#undef HAVE_SYS_STAT_H$'/'#define HAVE_SYS_STAT_H 1'/ \
    -e s/'^#undef HAVE_SYS_TIME_H$'/'#define HAVE_SYS_TIME_H 1'/ \
    -e s/'^#undef HAVE_SYS_TYPES_H$'/'#define HAVE_SYS_TYPES_H 1'/ \
    -e s/'^#undef HAVE_UINT16_T$'/'#define HAVE_UINT16_T 1'/ \
    -e s/'^#undef HAVE_UNISTD_H$'/'#define HAVE_UNISTD_H 1'/ \
    -e s/'^#undef HAVE_U_INT16_T$'/'#define HAVE_U_INT16_T 1'/ \
    -e s/'^#undef HAVE___ATTRIBUTE__$'/'#define HAVE___ATTRIBUTE__ 1'/ \
    -e s/'^#undef HAVE_WORKING_KQUEUE$'/'#define HAVE_WORKING_KQUEUE 1'/ \
    -e s/'^#undef PACKAGE$'/'#define PACKAGE "open-vcdiff"'/ \
    -e s/'^#undef PACKAGE_BUGREPORT$'/'#define PACKAGE_BUGREPORT "opensource@google.com"'/ \
    -e s/'^#undef PACKAGE_NAME$'/'#define PACKAGE_NAME "open-vcdiff"'/ \
    -e s/'^#undef PACKAGE_STRING$'/'#define PACKAGE_STRING "open-vcdiff 0.1"'/ \
    -e s/'^#undef PACKAGE_TARNAME$'/'#define PACKAGE_TARNAME "open-vcdiff"'/ \
    -e s/'^#undef PACKAGE_VERSION$'/'#define PACKAGE_VERSION "0.1"'/ \
    -e s/'^#undef STDC_HEADERS$'/'#define STDC_HEADERS 1'/ \
    -e s/'^#undef VERSION$'/'#define VERSION "0.1"'/ \
    -e s@'^\(#undef .*\)$'@'/* \1 */'@ \
  < "${CONFIG_H_IN}" \
  > "${CONFIG_H}.new"

if ! diff -q "${CONFIG_H}.new" "${CONFIG_H}" >& /dev/null ; then
  mv "${CONFIG_H}.new" "${CONFIG_H}"
else
  rm "${CONFIG_H}.new"
fi
