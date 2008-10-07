#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"

# Allow framework-style #include of <WebCore/WebCoreSystemInterface.h>
FORWARDING_DIR="${GENERATED_DIR}/webcoresysteminterface"
mkdir -p "${FORWARDING_DIR}"
cd "${FORWARDING_DIR}"
ln -fhs "../../../../../third_party/WebKit/WebCore/platform/mac" "WebCore"
