#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
mkdir -p "${GENERATED_DIR}"
cd build/JSConfig
exec bash create-config.sh "${GENERATED_DIR}" v8
