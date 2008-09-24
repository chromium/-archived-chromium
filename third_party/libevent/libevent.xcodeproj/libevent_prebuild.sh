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
  CONFIG_H_IN="config.h.in"
fi

CONFIG_H="${GENERATED_DIR}/config.h"
sed -e s/'^#undef DNS_USE_GETTIMEOFDAY_FOR_ID$'/'#define DNS_USE_GETTIMEOFDAY_FOR_ID 1'/ \
    -e s/'^#undef HAVE_DLFCN_H$'/'#define HAVE_DLFCN_H 1'/ \
    -e s/'^#undef HAVE_FCNTL$'/'#define HAVE_FCNTL 1'/ \
    -e s/'^#undef HAVE_FCNTL_H$'/'#define HAVE_FCNTL_H 1'/ \
    -e s/'^#undef HAVE_GETADDRINFO$'/'#define HAVE_GETADDRINFO 1'/ \
    -e s/'^#undef HAVE_GETNAMEINFO$'/'#define HAVE_GETNAMEINFO 1'/ \
    -e s/'^#undef HAVE_GETTIMEOFDAY$'/'#define HAVE_GETTIMEOFDAY 1'/ \
    -e s/'^#undef HAVE_INET_NTOP$'/'#define HAVE_INET_NTOP 1'/ \
    -e s/'^#undef HAVE_INTTYPES_H$'/'#define HAVE_INTTYPES_H 1'/ \
    -e s/'^#undef HAVE_KQUEUE$'/'#define HAVE_KQUEUE 1'/ \
    -e s/'^#undef HAVE_LIBRESOLV$'/'#define HAVE_LIBRESOLV 1'/ \
    -e s/'^#undef HAVE_MEMORY_H$'/'#define HAVE_MEMORY_H 1'/ \
    -e s/'^#undef HAVE_POLL$'/'#define HAVE_POLL 1'/ \
    -e s/'^#undef HAVE_POLL_H$'/'#define HAVE_POLL_H 1'/ \
    -e s/'^#undef HAVE_SELECT$'/'#define HAVE_SELECT 1'/ \
    -e s/'^#undef HAVE_SETFD$'/'#define HAVE_SETFD 1'/ \
    -e s/'^#undef HAVE_SIGACTION$'/'#define HAVE_SIGACTION 1'/ \
    -e s/'^#undef HAVE_SIGNAL$'/'#define HAVE_SIGNAL 1'/ \
    -e s/'^#undef HAVE_SIGNAL_H$'/'#define HAVE_SIGNAL_H 1'/ \
    -e s/'^#undef HAVE_STDARG_H$'/'#define HAVE_STDARG_H 1'/ \
    -e s/'^#undef HAVE_STDINT_H$'/'#define HAVE_STDINT_H 1'/ \
    -e s/'^#undef HAVE_STDLIB_H$'/'#define HAVE_STDLIB_H 1'/ \
    -e s/'^#undef HAVE_STRINGS_H$'/'#define HAVE_STRINGS_H 1'/ \
    -e s/'^#undef HAVE_STRING_H$'/'#define HAVE_STRING_H 1'/ \
    -e s/'^#undef HAVE_STRLCPY$'/'#define HAVE_STRLCPY 1'/ \
    -e s/'^#undef HAVE_STRSEP$'/'#define HAVE_STRSEP 1'/ \
    -e s/'^#undef HAVE_STRTOK_R$'/'#define HAVE_STRTOK_R 1'/ \
    -e s/'^#undef HAVE_STRTOLL$'/'#define HAVE_STRTOLL 1'/ \
    -e s/'^#undef HAVE_STRUCT_IN6_ADDR$'/'#define HAVE_STRUCT_IN6_ADDR 1'/ \
    -e s/'^#undef HAVE_SYS_EVENT_H$'/'#define HAVE_SYS_EVENT_H 1'/ \
    -e s/'^#undef HAVE_SYS_IOCTL_H$'/'#define HAVE_SYS_IOCTL_H 1'/ \
    -e s/'^#undef HAVE_SYS_PARAM_H$'/'#define HAVE_SYS_PARAM_H 1'/ \
    -e s/'^#undef HAVE_SYS_QUEUE_H$'/'#define HAVE_SYS_QUEUE_H 1'/ \
    -e s/'^#undef HAVE_SYS_SELECT_H$'/'#define HAVE_SYS_SELECT_H 1'/ \
    -e s/'^#undef HAVE_SYS_SOCKET_H$'/'#define HAVE_SYS_SOCKET_H 1'/ \
    -e s/'^#undef HAVE_SYS_STAT_H$'/'#define HAVE_SYS_STAT_H 1'/ \
    -e s/'^#undef HAVE_SYS_TIME_H$'/'#define HAVE_SYS_TIME_H 1'/ \
    -e s/'^#undef HAVE_SYS_TYPES_H$'/'#define HAVE_SYS_TYPES_H 1'/ \
    -e s/'^#undef HAVE_TAILQFOREACH$'/'#define HAVE_TAILQFOREACH 1'/ \
    -e s/'^#undef HAVE_TIMERADD$'/'#define HAVE_TIMERADD 1'/ \
    -e s/'^#undef HAVE_TIMERCLEAR$'/'#define HAVE_TIMERCLEAR 1'/ \
    -e s/'^#undef HAVE_TIMERCMP$'/'#define HAVE_TIMERCMP 1'/ \
    -e s/'^#undef HAVE_TIMERISSET$'/'#define HAVE_TIMERISSET 1'/ \
    -e s/'^#undef HAVE_UINT16_T$'/'#define HAVE_UINT16_T 1'/ \
    -e s/'^#undef HAVE_UINT32_T$'/'#define HAVE_UINT32_T 1'/ \
    -e s/'^#undef HAVE_UINT64_T$'/'#define HAVE_UINT64_T 1'/ \
    -e s/'^#undef HAVE_UINT8_T$'/'#define HAVE_UINT8_T 1'/ \
    -e s/'^#undef HAVE_UNISTD_H$'/'#define HAVE_UNISTD_H 1'/ \
    -e s/'^#undef HAVE_VASPRINTF$'/'#define HAVE_VASPRINTF 1'/ \
    -e s/'^#undef HAVE_WORKING_KQUEUE$'/'#define HAVE_WORKING_KQUEUE 1'/ \
    -e s/'^#undef PACKAGE$'/'#define PACKAGE "libevent"'/ \
    -e s/'^#undef PACKAGE_BUGREPORT$'/'#define PACKAGE_BUGREPORT ""'/ \
    -e s/'^#undef PACKAGE_NAME$'/'#define PACKAGE_NAME ""'/ \
    -e s/'^#undef PACKAGE_STRING$'/'#define PACKAGE_STRING ""'/ \
    -e s/'^#undef PACKAGE_TARNAME$'/'#define PACKAGE_TARNAME ""'/ \
    -e s/'^#undef PACKAGE_VERSION$'/'#define PACKAGE_VERSION ""'/ \
    -e s/'^#undef SIZEOF_INT$'/'#define SIZEOF_INT 4'/ \
    -e s/'^#undef SIZEOF_LONG$'/'#define SIZEOF_LONG 4'/ \
    -e s/'^#undef SIZEOF_LONG_LONG$'/'#define SIZEOF_LONG_LONG 8'/ \
    -e s/'^#undef SIZEOF_SHORT$'/'#define SIZEOF_SHORT 2'/ \
    -e s/'^#undef STDC_HEADERS$'/'#define STDC_HEADERS 1'/ \
    -e s/'^#undef TIME_WITH_SYS_TIME$'/'#define TIME_WITH_SYS_TIME 1'/ \
    -e s/'^#undef VERSION$'/'#define VERSION "1.4.7-stable"'/ \
    -e s@'^\(#undef .*\)$'@'/* \1 */'@ \
  < "${CONFIG_H_IN}" \
  > "${CONFIG_H}.new"

if ! diff -q "${CONFIG_H}.new" "${CONFIG_H}" >& /dev/null ; then
  mv "${CONFIG_H}.new" "${CONFIG_H}"
else
  rm "${CONFIG_H}.new"
fi
