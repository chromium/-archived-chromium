#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
mkdir -p "${GENERATED_DIR}"

# Generate the webkit version header
mkdir -p "${GENERATED_DIR}/include/v8/new"
python build/webkit_version.py \
       ../third_party/WebKit/WebCore/Configurations/Version.xcconfig \
       "${GENERATED_DIR}/include/v8/new"

# Only use new the file if it's different from the existing file (if any),
# preserving the existing file's timestamp when there are no changes.  This
# minimizes unnecessary build activity for a no-change build.
if ! diff -q "${GENERATED_DIR}/include/v8/new/webkit_version.h" \
             "${GENERATED_DIR}/include/v8/webkit_version.h" >& /dev/null ; then
  mv "${GENERATED_DIR}/include/v8/new/webkit_version.h" \
     "${GENERATED_DIR}/include/v8/webkit_version.h"
else
  rm "${GENERATED_DIR}/include/v8/new/webkit_version.h"
fi

rmdir "${GENERATED_DIR}/include/v8/new"

# TODO(mmentovai): fix this to not be so hokey - the Apple build expects
# JavaScriptCore to be in a framework  This belongs in the JSConfig target,
# which already does something similar
mkdir -p "${GENERATED_DIR}/include/v8/JavaScriptCore"
cp -p "${SRCROOT}/../third_party/WebKit/JavaScriptCore/bindings/npruntime.h" \
      "${GENERATED_DIR}/include/v8/JavaScriptCore"

export DerivedSourcesDir="${GENERATED_DIR}/DerivedSources/v8/WebCore"
mkdir -p "${GENERATED_DIR}/DerivedSources/v8/WebCore"
cd "${GENERATED_DIR}/DerivedSources/v8/WebCore"

# export CREATE_HASH_TABLE="${SRCROOT}/../third_party/WebKit/JavaScriptCore/kjs/create_hash_table"
# TODO(mmentovai): The above is normally correct, but create_hash_table wound
# up without the svn:executable property set in our repository.  See the TODO
# in jsbindings_prebuild.sh.
export CREATE_HASH_TABLE="${GENERATED_DIR}/create_hash_table"

ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore" WebCore
export WebCore="WebCore"
export SOURCE_ROOT="${WebCore}"
export PORTROOT="${SRCROOT}/port"

export PUBLICDOMINTERFACES="${PORTROOT}/../pending/PublicDOMInterfaces.h"
make -f "${SRCROOT}/pending/DerivedSources.make" \
     -j $(/usr/sbin/sysctl -n hw.ncpu)

# Allow framework-style #imports of <WebCore/whatever.h> to find the right
# headers.
cd ..
mkdir -p ForwardingHeaders/loader
ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore/loader" \
        ForwardingHeaders/loader/WebCore
