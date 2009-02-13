#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
GRIT_DIR="${GENERATED_DIR}/grit"
mkdir -p "${GRIT_DIR}"

# compare generated_resources.grd to generated_resources.h. If the .h is
# older or doesn't exist, rebuild it.
if [ "${GRIT_DIR}/generated_resources.h" -ot \
     "${PROJECT_DIR}/app/generated_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/app/generated_resources.grd" build \
      -o "${GRIT_DIR}"
fi

# compare chromium_strings.grd to chromium_strings.h. If the .h is
# older or doesn't exist, rebuild it
if [ "${GRIT_DIR}/chromium_strings.h" -ot \
     "${PROJECT_DIR}/app/chromium_strings.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/app/chromium_strings.grd" build \
      -o "${GRIT_DIR}"
fi

# compare browser_resources.grd to browser_resources.h. If the .h is
# older or doesn't exist, rebuild it
if [ "${GRIT_DIR}/browser_resources.h" -ot \
     "${PROJECT_DIR}/browser/browser_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/browser/browser_resources.grd" build \
      -o "${GRIT_DIR}"
fi

# compare theme_resources.grd to theme_resources.h. If the .h is
# older or doesn't exist, rebuild it
if [ "${GRIT_DIR}/grit/theme_resources.h" -ot \
     "${PROJECT_DIR}/app/theme/theme_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/app/theme/theme_resources.grd" build \
      -o "${GRIT_DIR}"
fi

# compare common_resources.grd to common_resources.h. If the .h is
# older or doesn't exist, rebuild it
if [ "${GRIT_DIR}/grit/common_resources.h" -ot \
     "${PROJECT_DIR}/common/common_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/common/common_resources.grd" build \
      -o "${GRIT_DIR}"
fi

# compare renderer_resources.grd to renderer_resources.h. If the .h is
# older or doesn't exist, rebuild it
if [ "${GRIT_DIR}/grit/renderer_resources.h" -ot \
     "${PROJECT_DIR}/renderer/renderer_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/renderer/renderer_resources.grd" build \
      -o "${GRIT_DIR}"
fi

# compare debugger_resources.grd to debugger_resources.h. If the .h is
# older or doesn't exist, rebuild it
if [ "${GRIT_DIR}/grit/debugger_resources.h" -ot \
     "${PROJECT_DIR}/browser/debugger/resources/debugger_resources.grd" ]
then
  python "${PROJECT_DIR}/../tools/grit/grit.py" \
      -i "${PROJECT_DIR}/browser/debugger/debugger/debugger_resources.grd" build \
      -o "${GRIT_DIR}"
fi
