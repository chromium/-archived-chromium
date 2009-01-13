#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
GRIT_DIR="${GENERATED_DIR}/grit"
mkdir -p "${GRIT_DIR}"

# compare webkit_strings.grd to webkit_strings.h. If the .h is
# older or doesn't exist, rebuild it.
if [ "${GRIT_DIR}/webkit_strings.h" -ot \
     "${PROJECT_DIR}/glue/webkit_strings.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/glue/webkit_strings.grd" build \
      -o "${GRIT_DIR}"
fi

if [ "${GRIT_DIR}/webkit_resources.h" -ot \
     "${PROJECT_DIR}/glue/webkit_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/glue/webkit_resources.grd" build \
      -o "${GRIT_DIR}"
fi
