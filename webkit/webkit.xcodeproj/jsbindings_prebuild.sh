#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
mkdir -p "${GENERATED_DIR}"

export PORTROOT="${SRCROOT}/port"

# export CREATE_HASH_TABLE="${SRCROOT}/../third_party/WebKit/JavaScriptCore/kjs/create_hash_table"
# TODO(mmentovai): The above is normally correct, but create_hash_table wound
# up without the svn:executable property set in our repository.  Until that's
# fixed - it should be fixed at the next WebKit merge following 2008-09-08 -
# make a copy of create_hash_table, set the executable bit on it, and use that.
# See also the TODO in webcore_prebuild.sh.
export CREATE_HASH_TABLE="${GENERATED_DIR}/create_hash_table"
cp -p "${SRCROOT}/../third_party/WebKit/JavaScriptCore/kjs/create_hash_table" \
      "${CREATE_HASH_TABLE}"
chmod a+x "${CREATE_HASH_TABLE}"

export DerivedSourcesDir="${GENERATED_DIR}/DerivedSources/v8/bindings"
mkdir -p "${DerivedSourcesDir}"
cd "${DerivedSourcesDir}"

ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore" WebCore
export WebCore="${DerivedSourcesDir}/WebCore"
export SOURCE_ROOT="${WebCore}"
export ENCODINGS_FILE="${WebCore}/platform/text/mac/mac-encodings.txt";
export ENCODINGS_PREFIX="kTextEncoding"

export PUBLICDOMINTERFACES="${PORTROOT}/PublicDOMInterfaces.h"
make -f "${PORTROOT}/DerivedSources.make" -j $(/usr/sbin/sysctl -n hw.ncpu)

# Allow framework-style #imports of <WebCore/whatever.h> to find the right
# headers
cd ..
mkdir -p ForwardingHeaders/Derived \
         ForwardingHeaders/dom \
         ForwardingHeaders/editing \
         ForwardingHeaders/ObjC \
         ForwardingHeaders/page_mac \
         ForwardingHeaders/svg
ln -sfh "${DerivedSourcesDir}" ForwardingHeaders/Derived/WebCore
ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore/dom" \
        ForwardingHeaders/dom/WebCore
ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore/editing" \
        ForwardingHeaders/editing/WebCore
ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore/bindings/objc" \
        ForwardingHeaders/ObjC/WebCore
ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore/page/mac" \
        ForwardingHeaders/page_mac/WebCore
ln -sfh "${SRCROOT}/../third_party/WebKit/WebCore/svg" \
        ForwardingHeaders/svg/WebCore
