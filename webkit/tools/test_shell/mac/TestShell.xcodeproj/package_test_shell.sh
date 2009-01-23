#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Packages TestShell up into a disk image, renaming it "Chromium TestShell"
# so that it's easily identifiable.
#
# TODO(mmentovai): This is temporary, as soon as we can package up Chromium
# proper, we should rip this out and let TestShell live out the rest of its
# years as a simple test shell.

set -ex

TOP="${SRCROOT}/../../../.."
PKG_DMG="${TOP}/build/mac/pkg-dmg"
KEYSTONE_URL="https://tools.google.com/service/update2"

SRC_APP_NAME="TestShell"
DST_APP_NAME="Chromium TestShell"
SRC_APP_PATH="${BUILT_PRODUCTS_DIR}/${SRC_APP_NAME}.app"

KSR="${TOP}/third_party/googlemac/Releases/Keystone/KeystoneRegistration.framework"

# Really, if you want to distribute something, it probably shouldn't be
# a debug-mode build.  Release-mode builds are already stripped and are
# intended for this sort of thing.
if [ "${CONFIGURATION}" != "Release" ] ; then
  echo "warning: packaging in non-release mode" >&2
fi

# Figure out what version this build corresponds to.  Just use the svn revision
# for now.
SVN_REVISION=$(svnversion "${TOP}" | sed -e "s/[^0-9]//g")
if [ -z "${SVN_REVISION}" ] ; then
  echo "warning: could not determine svn revision" >&2
fi

# I really hate how "defaults" doesn't take a real pathname but instead insists
# on appending ".plist" to everything.
INFO_PLIST_PATH="Contents/Info.plist"
TMP_INFO_PLIST_DEFAULTS="${TEMP_DIR}/Info"
TMP_INFO_PLIST="${TMP_INFO_PLIST_DEFAULTS}.plist"
cp "${SRC_APP_PATH}/${INFO_PLIST_PATH}" "${TMP_INFO_PLIST}"

# Stuff the version information in the Info.plist, and update the CFBundleName
# to correspond to what we want to call it.
defaults write "${TMP_INFO_PLIST_DEFAULTS}" CFBundleName "${DST_APP_NAME}"
defaults write "${TMP_INFO_PLIST_DEFAULTS}" SVNRevision "0.0.${SVN_REVISION}"
if [ -d "${KSR}" ] ; then
  defaults write "${TMP_INFO_PLIST_DEFAULTS}" KSUpdateURL "${KEYSTONE_URL}"
fi

# Info.plist will work perfectly well in any plist format, but traditionally
# applications use xml1 for this, so convert it back after whatever defaults
# might have done.
plutil -convert xml1 "${TMP_INFO_PLIST}"

# If the Keystone registration framework is available, stuff it and the
# Keystone installation script into the application, so that the result will
# be both auto-updatable and suitable for use as the target of an update.
PKG_DMG_EXTRA=()
if [ -d "${KSR}" ] ; then
  PKG_DMG_EXTRA=(--copy "${KSR}:/${DST_APP_NAME}.app/Contents/Frameworks" \
                 --copy "${SRCROOT}/keystone_install.sh:/.keystone_install")
fi

"${PKG_DMG}" --source /var/empty \
             --target "${BUILT_PRODUCTS_DIR}/ChromiumTestShell.dmg" \
             --format UDBZ \
             --volname "${DST_APP_NAME}" \
             --tempdir "${TEMP_DIR}" \
             --copy "${SRC_APP_PATH}/:/${DST_APP_NAME}.app/" \
             --copy "${TMP_INFO_PLIST}:/${DST_APP_NAME}.app/${INFO_PLIST_PATH}" \
             "${PKG_DMG_EXTRA[@]}"
