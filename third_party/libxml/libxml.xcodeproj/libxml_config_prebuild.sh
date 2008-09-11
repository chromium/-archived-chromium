#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"

PACKAGE=libxml2
VERSION_MAJOR=2
VERSION_MINOR=6
VERSION_MICRO=31
VERSION_STRING="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO}"
VERSION_NUMBER=$(printf "%d%02d%02d" \
                        "${VERSION_MAJOR}" \
                        "${VERSION_MINOR}" \
                        "${VERSION_EXTRA}")

# Generate xmlversion.h the same way that "sh configure" would
mkdir -p "${GENERATED_DIR}/include/libxml"
XMLVERSION_H="${GENERATED_DIR}/include/libxml/xmlversion.h"
sed -e s/@VERSION@/"${VERSION_STRING}"/g \
    -e s/@LIBXML_VERSION_NUMBER@/"${VERSION_NUMBER}"/g \
    -e s/@LIBXML_VERSION_EXTRA@//g \
    -e s/@WITH_TRIO@/0/g \
    -e s/@WITH_THREADS@/1/g \
    -e s/@WITH_TREE@/1/g \
    -e s/@WITH_OUTPUT@/1/g \
    -e s/@WITH_PUSH@/1/g \
    -e s/@WITH_READER@/1/g \
    -e s/@WITH_PATTERN@/1/g \
    -e s/@WITH_WRITER@/1/g \
    -e s/@WITH_SAX1@/1/g \
    -e s/@WITH_FTP@/1/g \
    -e s/@WITH_HTTP@/1/g \
    -e s/@WITH_VALID@/1/g \
    -e s/@WITH_HTML@/1/g \
    -e s/@WITH_LEGACY@/1/g \
    -e s/@WITH_C14N@/1/g \
    -e s/@WITH_CATALOG@/1/g \
    -e s/@WITH_DOCB@/1/g \
    -e s/@WITH_XPATH@/1/g \
    -e s/@WITH_XPTR@/1/g \
    -e s/@WITH_XINCLUDE@/1/g \
    -e s/@WITH_ICONV@/0/g \
    -e s/@WITH_ICU@/1/g \
    -e s/@WITH_ISO8859X@/1/g \
    -e s/@WITH_DEBUG@/1/g \
    -e s/@WITH_MEM_DEBUG@/0/g \
    -e s/@WITH_RUN_DEBUG@/0/g \
    -e s/@WITH_REGEXPS@/1/g \
    -e s/@WITH_SCHEMAS@/1/g \
    -e s/@WITH_SCHEMATRON@/1/g \
    -e s/@WITH_MODULES@/1/g \
    -e s/@MODULE_EXTENSION@/.so/g \
    -e s/@WITH_ZLIB@/1/g \
  < include/libxml/xmlversion.h.in \
  > "${XMLVERSION_H}.new"

# Only use the new file if it's different from the existing file (if any),
# preserving the existing file's timestamp when there are no changes to
# minimize unnecessary build activity.
if ! diff -q "${XMLVERSION_H}.new" "${XMLVERSION_H}" >& /dev/null ; then
  mv "${XMLVERSION_H}.new" "${XMLVERSION_H}"
else
  rm "${XMLVERSION_H}.new"
fi

CONFIG_H="${GENERATED_DIR}/config.h"
sed -e s/'^#undef PACKAGE$'/"#define PACKAGE \"${PACKAGE}\""/ \
    -e s/'^#undef VERSION$'/"#define VERSION \"${VERSION_STRING}\""/ \
    -e s/'^#undef SUPPORT_IPV6$'/"#define SUPPORT_IPV6"/ \
    -e s/'^#undef HAVE_ARPA_INET_H$'/"#define HAVE_ARPA_INET_H 1"/ \
    -e s/'^#undef HAVE_ARPA_NAMESER_H$'/"#define HAVE_ARPA_NAMESER_H 1"/ \
    -e s/'^#undef HAVE_CTYPE_H$'/"#define HAVE_CTYPE_H 1"/ \
    -e s/'^#undef HAVE_DIRENT_H$'/"#define HAVE_DIRENT_H 1"/ \
    -e s/'^#undef HAVE_DLFCN_H$'/"#define HAVE_DLFCN_H 1"/ \
    -e s/'^#undef HAVE_DLOPEN$'/"#define HAVE_DLOPEN "/ \
    -e s/'^#undef HAVE_ERRNO_H$'/"#define HAVE_ERRNO_H 1"/ \
    -e s/'^#undef HAVE_FCNTL_H$'/"#define HAVE_FCNTL_H 1"/ \
    -e s/'^#undef HAVE_FINITE$'/"#define HAVE_FINITE 1"/ \
    -e s/'^#undef HAVE_FLOAT_H$'/"#define HAVE_FLOAT_H 1"/ \
    -e s/'^#undef HAVE_FPRINTF$'/"#define HAVE_FPRINTF 1"/ \
    -e s/'^#undef HAVE_FTIME$'/"#define HAVE_FTIME 1"/ \
    -e s/'^#undef HAVE_GETADDRINFO$'/"#define HAVE_GETADDRINFO "/ \
    -e s/'^#undef HAVE_GETTIMEOFDAY$'/"#define HAVE_GETTIMEOFDAY 1"/ \
    -e s/'^#undef HAVE_INTTYPES_H$'/"#define HAVE_INTTYPES_H 1"/ \
    -e s/'^#undef HAVE_ISINF$'/"#define HAVE_ISINF "/ \
    -e s/'^#undef HAVE_ISNAN$'/"#define HAVE_ISNAN "/ \
    -e s/'^#undef HAVE_LIBPTHREAD$'/"#define HAVE_LIBPTHREAD "/ \
    -e s/'^#undef HAVE_LIBZ$'/"#define HAVE_LIBZ 1"/ \
    -e s/'^#undef HAVE_LIMITS_H$'/"#define HAVE_LIMITS_H 1"/ \
    -e s/'^#undef HAVE_LOCALTIME$'/"#define HAVE_LOCALTIME 1"/ \
    -e s/'^#undef HAVE_MATH_H$'/"#define HAVE_MATH_H 1"/ \
    -e s/'^#undef HAVE_MEMORY_H$'/"#define HAVE_MEMORY_H 1"/ \
    -e s/'^#undef HAVE_NETDB_H$'/"#define HAVE_NETDB_H 1"/ \
    -e s/'^#undef HAVE_NETINET_IN_H$'/"#define HAVE_NETINET_IN_H 1"/ \
    -e s/'^#undef HAVE_PRINTF$'/"#define HAVE_PRINTF 1"/ \
    -e s/'^#undef HAVE_PTHREAD_H$'/"#define HAVE_PTHREAD_H "/ \
    -e s/'^#undef HAVE_RESOLV_H$'/"#define HAVE_RESOLV_H 1"/ \
    -e s/'^#undef HAVE_SIGNAL$'/"#define HAVE_SIGNAL 1"/ \
    -e s/'^#undef HAVE_SIGNAL_H$'/"#define HAVE_SIGNAL_H 1"/ \
    -e s/'^#undef HAVE_SNPRINTF$'/"#define HAVE_SNPRINTF 1"/ \
    -e s/'^#undef HAVE_SPRINTF$'/"#define HAVE_SPRINTF 1"/ \
    -e s/'^#undef HAVE_SSCANF$'/"#define HAVE_SSCANF 1"/ \
    -e s/'^#undef HAVE_STAT$'/"#define HAVE_STAT 1"/ \
    -e s/'^#undef HAVE_STDARG_H$'/"#define HAVE_STDARG_H 1"/ \
    -e s/'^#undef HAVE_STDINT_H$'/"#define HAVE_STDINT_H 1"/ \
    -e s/'^#undef HAVE_STDLIB_H$'/"#define HAVE_STDLIB_H 1"/ \
    -e s/'^#undef HAVE_STRDUP$'/"#define HAVE_STRDUP 1"/ \
    -e s/'^#undef HAVE_STRERROR$'/"#define HAVE_STRERROR 1"/ \
    -e s/'^#undef HAVE_STRFTIME$'/"#define HAVE_STRFTIME 1"/ \
    -e s/'^#undef HAVE_STRINGS_H$'/"#define HAVE_STRINGS_H 1"/ \
    -e s/'^#undef HAVE_STRING_H$'/"#define HAVE_STRING_H 1"/ \
    -e s/'^#undef HAVE_SYS_MMAN_H$'/"#define HAVE_SYS_MMAN_H 1"/ \
    -e s/'^#undef HAVE_SYS_SELECT_H$'/"#define HAVE_SYS_SELECT_H 1"/ \
    -e s/'^#undef HAVE_SYS_SOCKET_H$'/"#define HAVE_SYS_SOCKET_H 1"/ \
    -e s/'^#undef HAVE_SYS_STAT_H$'/"#define HAVE_SYS_STAT_H 1"/ \
    -e s/'^#undef HAVE_SYS_TIMEB_H$'/"#define HAVE_SYS_TIMEB_H 1"/ \
    -e s/'^#undef HAVE_SYS_TIME_H$'/"#define HAVE_SYS_TIME_H 1"/ \
    -e s/'^#undef HAVE_SYS_TYPES_H$'/"#define HAVE_SYS_TYPES_H 1"/ \
    -e s/'^#undef HAVE_TIME_H$'/"#define HAVE_TIME_H 1"/ \
    -e s/'^#undef HAVE_UNISTD_H$'/"#define HAVE_UNISTD_H 1"/ \
    -e s/'^#undef HAVE_VA_COPY$'/"#define HAVE_VA_COPY 1"/ \
    -e s/'^#undef HAVE_VFPRINTF$'/"#define HAVE_VFPRINTF 1"/ \
    -e s/'^#undef HAVE_VSNPRINTF$'/"#define HAVE_VSNPRINTF 1"/ \
    -e s/'^#undef HAVE_VSPRINTF$'/"#define HAVE_VSPRINTF 1"/ \
    -e s/'^#undef HAVE_ZLIB_H$'/"#define HAVE_ZLIB_H 1"/ \
    -e s/'^#undef PACKAGE_BUGREPORT$'/"#define PACKAGE_BUGREPORT \"\""/ \
    -e s/'^#undef PACKAGE_NAME$'/"#define PACKAGE_NAME \"\""/ \
    -e s/'^#undef PACKAGE_STRING$'/"#define PACKAGE_STRING \"\""/ \
    -e s/'^#undef PACKAGE_TARNAME$'/"#define PACKAGE_TARNAME \"\""/ \
    -e s/'^#undef PACKAGE_VERSION$'/"#define PACKAGE_VERSION \"\""/ \
    -e s/'^#undef PROTOTYPES$'/"#define PROTOTYPES 1"/ \
    -e s/'^#undef STDC_HEADERS$'/"#define STDC_HEADERS 1"/ \
    -e s/'^#undef SUPPORT_IP6$'/"#define SUPPORT_IP6 "/ \
    -e s/'^#undef XML_SOCKLEN_T$'/"#define XML_SOCKLEN_T socklen_t"/ \
    -e s/'^#undef __PROTOTYPES$'/"#define __PROTOTYPES 1"/ \
    -e s@'^\(#undef .*\)$'@'/* \1 */'@ \
  < config.h.in \
  > "${CONFIG_H}.new"

if ! diff -q "${CONFIG_H}.new" "${CONFIG_H}" >& /dev/null ; then
  mv "${CONFIG_H}.new" "${CONFIG_H}"
else
  rm "${CONFIG_H}.new"
fi
