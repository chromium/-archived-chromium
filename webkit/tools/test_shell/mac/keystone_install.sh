#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called by the Keystone system to update the installed application with a new
# version from a disk image.

set -e

# The argument should be the disk image path.  Make sure it exists.
if [ ! -d "${1}" ] ; then
  exit 1
fi

# Who we are.
APP_NAME="Chromium TestShell.app"
SRC="${1}/${APP_NAME}"

# Sanity, make sure that there's something to copy from.
if [ ! -d "${SRC}" ] ; then
  exit 1
fi

# Figure out where we're going.
BUNDLE_ID=$(defaults read "${SRC}/Contents/Info" CFBundleIdentifier)
DEST=$(ksadmin -pP "${BUNDLE_ID}" | grep xc= | sed -E 's/.+path=(.+)>$/\1/g')

# More sanity checking.
if [ -z "${SRC}" ] || [ -z "${DEST}" ] || [ ! -d $(dirname "${DEST}") ]; then
  exit 1
fi

# This usage will preserve any changes the user made to the application name.
rsync -a --delete "${SRC}/" "${DEST}/"

VERSION=$(defaults read "${DEST}/Contents/Info" SVNRevision)
URL=$(defaults read "${DEST}/Contents/Info" KSUpdateURL)

# Notify LaunchServices.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister "${DEST}"

# Notify Keystone.
ksadmin -P "${BUNDLE_ID}" \
        --version "${VERSION}" \
        --xcpath "${DEST}" \
        --url "${URL}"

# Great success!
exit 0
