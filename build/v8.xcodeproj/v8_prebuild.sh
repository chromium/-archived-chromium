#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
JS_FILES="runtime.js \
          v8natives.js \
          array.js \
          string.js \
          uri.js \
          math.js \
          messages.js \
          apinatives.js \
          debug-delay.js \
          mirror-delay.js \
          date-delay.js \
          regexp-delay.js \
          macros.py"

V8ROOT="${SRCROOT}/../v8"

SRC_DIR="${V8ROOT}/src"

NATIVE_JS_FILES=""

for i in ${JS_FILES} ; do
  NATIVE_JS_FILES+="${SRC_DIR}/${i} "
done

V8_GENERATED_SOURCES_DIR="${CONFIGURATION_TEMP_DIR}/generated"
mkdir -p "${V8_GENERATED_SOURCES_DIR}"

LIBRARIES_CC="${V8_GENERATED_SOURCES_DIR}/libraries.cc"
LIBRARIES_EMPTY_CC="${V8_GENERATED_SOURCES_DIR}/libraries-empty.cc"

python "${V8ROOT}/tools/js2c.py" \
  "${LIBRARIES_CC}.new" \
  "${LIBRARIES_EMPTY_CC}.new" \
  CORE \
  ${NATIVE_JS_FILES}

# Only use the new files if they're different from the existing files (if any),
# preserving the existing files' timestamps when there are no changes.  This
# minimizes unnecessary build activity for a no-change build.

if ! diff -q "${LIBRARIES_CC}.new" "${LIBRARIES_CC}" >& /dev/null
then
  mv "${LIBRARIES_CC}.new" "${LIBRARIES_CC}"
else
  rm "${LIBRARIES_CC}.new"
fi

if ! diff -q "${LIBRARIES_EMPTY_CC}.new" "${LIBRARIES_EMPTY_CC}" >& /dev/null
then
  mv "${LIBRARIES_EMPTY_CC}.new" "${LIBRARIES_EMPTY_CC}"
else
  rm "${LIBRARIES_EMPTY_CC}.new"
fi
