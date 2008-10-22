#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"

PACKAGE=libxslt
VERSION_MAJOR=1
VERSION_MINOR=1
VERSION_MICRO=24
VERSION_STRING="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}"

# Generate config.h the same way that "sh configure" would
CONFIG_H="${GENERATED_DIR}/config.h"
sed -e s/'^#undef WITH_DEBUGGER$'/"#define WITH_DEBUGGER 1"/ \
    -e s/'^#undef HAVE_ASCTIME'/"#define HAVE_ASCTIME 1"/ \
    -e s/'^#undef HAVE_DLFCN_H$'/"#define HAVE_DLFCN_H 1"/ \
    -e s/'^#undef HAVE_FLOAT_H$'/"#define HAVE_FLOAT_H 1"/ \
    -e s/'^#undef HAVE_FPRINTF$'/"#define HAVE_FPRINTF 1"/ \
    -e s/'^#undef HAVE_FTIME$'/"#define HAVE_FTIME 1"/ \
    -e s/'^#undef HAVE_GETTIMEOFDAY$'/"#define HAVE_GETTIMEOFDAY 1"/ \
    -e s/'^#undef HAVE_GMTIME_R$'/"#define HAVE_GMTIME_R 1"/ \
    -e s/'^#undef HAVE_INTTYPES_H$'/"#define HAVE_INTTYPES_H 1"/ \
    -e s/'^#undef HAVE_LOCALTIME$'/"#define HAVE_LOCALTIME 1"/ \
    -e s/'^#undef HAVE_LOCALTIME_R$'/"#define HAVE_LOCALTIME_R 1"/ \
    -e s/'^#undef HAVE_MATH_H$'/"#define HAVE_MATH_H 1"/ \
    -e s/'^#undef HAVE_MEMORY_H$'/"#define HAVE_MEMORY_H 1"/ \
    -e s/'^#undef HAVE_MKTIME$'/"#define HAVE_MKTIME 1"/ \
    -e s/'^#undef HAVE_PRINTF$'/"#define HAVE_PRINTF 1"/ \
    -e s/'^#undef HAVE_SNPRINTF$'/"#define HAVE_SNPRINTF 1"/ \
    -e s/'^#undef HAVE_SPRINTF$'/"#define HAVE_SPRINTF 1"/ \
    -e s/'^#undef HAVE_SSCANF$'/"#define HAVE_SSCANF 1"/ \
    -e s/'^#undef HAVE_STAT$'/"#define HAVE_STAT 1"/ \
    -e s/'^#undef HAVE_STDARG_H$'/"#define HAVE_STDARG_H 1"/ \
    -e s/'^#undef HAVE_STDINT_H$'/"#define HAVE_STDINT_H 1"/ \
    -e s/'^#undef HAVE_STDLIB_H$'/"#define HAVE_STDLIB_H 1"/ \
    -e s/'^#undef HAVE_STRINGS_H$'/"#define HAVE_STRINGS_H 1"/ \
    -e s/'^#undef HAVE_STRING_H$'/"#define HAVE_STRING_H 1"/ \
    -e s/'^#undef HAVE_SYS_SELECT_H$'/"#define HAVE_SYS_SELECT_H 1"/ \
    -e s/'^#undef HAVE_SYS_STAT_H$'/"#define HAVE_SYS_STAT_H 1"/ \
    -e s/'^#undef HAVE_SYS_TIMEB_H$'/"#define HAVE_SYS_TIMEB_H 1"/ \
    -e s/'^#undef HAVE_SYS_TIME_H$'/"#define HAVE_SYS_TIME_H 1"/ \
    -e s/'^#undef HAVE_SYS_TYPES_H$'/"#define HAVE_SYS_TYPES_H 1"/ \
    -e s/'^#undef HAVE_TIME$'/"#define HAVE_TIME 1"/ \
    -e s/'^#undef HAVE_TIME_H$'/"#define HAVE_TIME_H 1"/ \
    -e s/'^#undef HAVE_UNISTD_H$'/"#define HAVE_UNISTD_H 1"/ \
    -e s/'^#undef HAVE_VFPRINTF$'/"#define HAVE_VFPRINTF 1"/ \
    -e s/'^#undef HAVE_VSNPRINTF$'/"#define HAVE_VSNPRINTF 1"/ \
    -e s/'^#undef HAVE_VSPRINTF$'/"#define HAVE_VSPRINTF 1"/ \
    -e s/'^#undef PACKAGE$'/"#define PACKAGE \"libxslt\""/ \
    -e s/'^#undef PACKAGE_BUGREPORT$'/"#define PACKAGE_BUGREPORT \"\""/ \
    -e s/'^#undef PACKAGE_NAME$'/"#define PACKAGE_NAME \"\""/ \
    -e s/'^#undef PACKAGE_STRING$'/"#define PACKAGE_STRING \"\""/ \
    -e s/'^#undef PACKAGE_TARNAME$'/"#define PACKAGE_TARNAME \"\""/ \
    -e s/'^#undef PACKAGE_VERSION$'/"#define PACKAGE_VERSION \"\""/ \
    -e s/'^#undef STDC_HEADERS$'/"#define STDC_HEADERS 1"/ \
    -e s/'^#undef VERSION$'/"#define VERSION \"${VERSION_STRING}\""/ \
    -e s@'^\(#undef .*\)$'@'/* \1 */'@ \
  < config.h.in \
  > "${CONFIG_H}.new"

# Only use the new file if it's different from the existing file (if any),
# preserving the existing file's timestamp when there are no changes to
# minimize unnecessary build activity.
if ! diff -q "${CONFIG_H}.new" "${CONFIG_H}" >& /dev/null ; then
  mv "${CONFIG_H}.new" "${CONFIG_H}"
else
  rm "${CONFIG_H}.new"
fi
