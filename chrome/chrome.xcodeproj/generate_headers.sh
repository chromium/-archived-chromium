#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
GRIT_DIR="${GENERATED_DIR}/grit"
mkdir -p "${GRIT_DIR}"

python "${PROJECT_DIR}/../tools/grit/grit.py" \
    -i "${PROJECT_DIR}/app/generated_resources.grd" build \
    -o "${GRIT_DIR}"

python "${PROJECT_DIR}/../tools/grit/grit.py" \
    -i "${PROJECT_DIR}/app/chromium_strings.grd" build \
    -o "${GRIT_DIR}"
