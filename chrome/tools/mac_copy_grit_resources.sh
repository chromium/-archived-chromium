#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
 
set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
REPACKED_DIR="${GENERATED_DIR}/grit_repacked"
APP_RESOURCES_ROOT_DIR="${TARGET_BUILD_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"
mkdir -p "${APP_RESOURCES_ROOT_DIR}"


if [ "${APP_RESOURCES_ROOT_DIR}/chrome.pak" -ot \
     "${REPACKED_DIR}/chrome.pak" ]
then
  cp -f "${REPACKED_DIR}/chrome.pak" \
      "${APP_RESOURCES_ROOT_DIR}/chrome.pak"
fi

if [ "${APP_RESOURCES_ROOT_DIR}/theme.pak" -ot \
     "${REPACKED_DIR}/theme.pak" ]
then
  cp -f "${REPACKED_DIR}/theme.pak" \
      "${APP_RESOURCES_ROOT_DIR}/theme.pak"
fi

# TODO: this should loop though all the languages and copy them to the
# right folder (note the name change)

if [ "${APP_RESOURCES_ROOT_DIR}/locale.pak" -ot \
     "${REPACKED_DIR}/locale_en-US.pak" ]
then
  mkdir -p "${APP_RESOURCES_ROOT_DIR}/en.lproj"
  cp -f "${REPACKED_DIR}/locale_en-US.pak" \
      "${APP_RESOURCES_ROOT_DIR}/en.lproj/locale.pak"
fi
