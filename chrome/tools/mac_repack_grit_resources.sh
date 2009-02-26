#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
 
set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
GRIT_DIR="${GENERATED_DIR}/grit"
REPACKED_DIR="${GENERATED_DIR}/grit_repacked"
mkdir -p "${REPACKED_DIR}"

# TODO: these need dependency checks

python "${PROJECT_DIR}/../tools/data_pack/repack.py" \
    "${REPACKED_DIR}/chrome.pak" \
    "${GRIT_DIR}/browser_resources.pak" \
    "${GRIT_DIR}/debugger_resources.pak" \
    "${GRIT_DIR}/common_resources.pak" \
    "${GRIT_DIR}/renderer_resources.pak"

# Need two to repack, but linux avoids that by directly invoking the module, so
# we just cp instead.
cp -f "${GRIT_DIR}/theme_resources.pak" "${REPACKED_DIR}/theme.pak"
    

# TODO: we when these are built for each language, we need to loop over
# the languages and do the repack on all of them.

python "${PROJECT_DIR}/../tools/data_pack/repack.py" \
    "${REPACKED_DIR}/locale_en-US.pak" \
    "${GRIT_DIR}/generated_resources_en-US.pak" \
    "${GRIT_DIR}/chromium_strings_en-US.pak" \
    "${GRIT_DIR}/locale_settings_en-US.pak"
