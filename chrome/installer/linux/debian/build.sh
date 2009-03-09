#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO
# - only package opt builds?

set -e
if [ "$VERBOSE" ]; then
  set -x
fi

# Recursively replace @@include@@ template variables with the referenced file,
# and write the resulting text to stdout.
process_template_includes() {
  INCSTACK+="$1->"
  # Includes are relative to the file that does the include.
  INCDIR=$(dirname $1)
  # Clear IFS so 'read' doesn't trim whitespace
  local OLDIFS="$IFS"
  IFS=''
  while read -r LINE
  do
    INCLINE=$(sed -e '/@@include@@/!d' <<<$LINE)
    if [ -n "$INCLINE" ]; then
      INCFILE=$(echo $INCLINE | sed -e "s#@@include@@\(.*\)#\1#")
      # Simple filename match to detect cyclic includes.
      CYCLE=$(sed -e "\#$INCFILE#"'!d' <<<$INCSTACK)
      if [ "$CYCLE" ]; then
        echo "ERROR: Possible cyclic include detected." 1>&2
        echo "$INCSTACK$INCFILE" 1>&2
        exit 1
      fi
      process_template_includes "$INCDIR/$INCFILE"
    else
      echo "$LINE"
    fi
  done < "$1"
  IFS="$OLDIFS"
  INCSTACK=${INCSTACK%"$1->"}
}

# Replace template variables (@@VARNAME@@) in the given template file. If a
# second argument is given, save the processed text to that filename, otherwise
# modify the template file in place.
process_template() {
  local TMPLIN="$1"
  if [ -z "$2" ]; then
    local TMPLOUT="$TMPLIN"
  else
    local TMPLOUT="$2"
  fi
  # Process includes first so included text also gets substitutions.
  TMPLINCL="$(process_template_includes "$TMPLIN")"
  sed \
    -e "s#@@PACKAGE@@#${PACKAGE}#g" \
    -e "s#@@PROGNAME@@#${PROGNAME}#g" \
    -e "s#@@VERSION@@#${VERSIONFULL}#g" \
    -e "s#@@INSTALLDIR@@#${INSTALLDIR}#g" \
    -e "s#@@MENUNAME@@#${MENUNAME}#g" \
    -e "s#@@DEPENDS@@#${DEB_DEPS}#g" \
    -e "s#@@PROVIDES@@#${DEB_PROVIDES}#g" \
    -e "s#@@REPLACES@@#${DEB_REPLACES}#g" \
    -e "s#@@CONFLICTS@@#${DEB_CONFLICTS}#g" \
    -e "s#@@ARCHITECTURE@@#${DEB_HOST_ARCH}#g" \
    -e "s#@@DEBFULLNAME@@#${DEBFULLNAME}#g" \
    -e "s#@@DEBEMAIL@@#${DEBEMAIL}#g" \
    -e "s#@@REPOCONFIG@@#${REPOCONFIG}#g" \
    -e "s#@@SHORTDESC@@#${SHORTDESC}#g" \
    -e "s#@@FULLDESC@@#${FULLDESC}#g" \
    > "$TMPLOUT" <<< "$TMPLINCL"
}

# Create the Debian changelog file needed by dpkg-gencontrol. This just adds a
# placeholder change, indicating it is the result of an automatic build.
# TODO(mmoss) Release packages should create something meaningful for a
# changelog, but simply grabbing the actual 'svn log' is way too verbose. Do we
# have any type of "significant/visible changes" log that we could use for this?
gen_changelog() {
  rm -f "${CHANGELOG}"
  process_template changelog.template "${CHANGELOG}"
  debchange -a --nomultimaint -m --changelog "${CHANGELOG}" \
    --distribution UNRELEASED "automatic build"
}

# Create the Debian control file needed by dpkg-deb.
gen_control() {
  dpkg-gencontrol -v"${VERSIONFULL}" -ccontrol -l"${CHANGELOG}" -ffiles \
    -p"${PACKAGE}" -P"${STAGEDIR}" -O > "${STAGEDIR}/DEBIAN/control"
  rm -f control
}

# Setup the installation directory hierachy in the package staging area.
prep_staging() {
  install -m 755 -d "${STAGEDIR}/DEBIAN" \
    "${STAGEDIR}${INSTALLDIR}" \
    "${STAGEDIR}/${INSTALLDIR}/locales" \
    "${STAGEDIR}/${INSTALLDIR}/themes" \
    "${STAGEDIR}/usr/bin"
}

# Put the package contents in the staging area.
stage_install() {
  echo "Staging install files in '${STAGEDIR}'..."
  # TODO(mmoss) This assumes we built the static binaries. To support shared
  # builds, we probably want an install target in scons so it can give us all
  # the right files. See also:
  # http://code.google.com/p/chromium/issues/detail?id=4451
  install -m 755 -s "${BUILDDIR}/${PROGNAME}" "${STAGEDIR}${INSTALLDIR}/"
  install -m 644 "${BUILDDIR}/${PROGNAME}.pak" "${STAGEDIR}${INSTALLDIR}/"
  install -m 644 "${BUILDDIR}/locales/en-US.pak" \
    "${STAGEDIR}${INSTALLDIR}/locales/"
  install -m 644 "${BUILDDIR}/themes/default.pak" \
    "${STAGEDIR}${INSTALLDIR}/themes/"
  install -m 644 "${BUILDDIR}/icudt38l.dat" "${STAGEDIR}${INSTALLDIR}/"
  install -m 644 "../common/${PACKAGE}/${PACKAGE}.png" \
    "${STAGEDIR}${INSTALLDIR}/"
  process_template "../common/wrapper" "${STAGEDIR}${INSTALLDIR}/${PACKAGE}"
  chmod 755 "${STAGEDIR}${INSTALLDIR}/${PACKAGE}"
  (cd "${STAGEDIR}/usr/bin/" && ln -snf "${INSTALLDIR}/${PACKAGE}" "${PACKAGE}")
  process_template "../common/desktop.template" \
    "${STAGEDIR}${INSTALLDIR}/${PACKAGE}.desktop"
  chmod 644 "${STAGEDIR}${INSTALLDIR}/${PACKAGE}.desktop"
  process_template postinst "${STAGEDIR}/DEBIAN/postinst"
  chmod 755 "${STAGEDIR}/DEBIAN/postinst"
  process_template prerm "${STAGEDIR}/DEBIAN/prerm"
  chmod 755 "${STAGEDIR}/DEBIAN/prerm"
  # TODO(mmoss) For dogfooding, we already have daily updates of all installed
  # packages. Externally, we'll have to implement something if we don't want to
  # rely exclusively on the system updater (the default configuration is often
  # not forced/silent).
  #process_template "../common/updater" \
  #  "${STAGEDIR}/etc/cron.daily/${PACKAGE}-updater"
  #chmod 755 "${STAGEDIR}/etc/cron.daily/${PACKAGE}-updater"
}

# Put the 32-bit compatability libraries in the staging area. This is a
# workaround for missing libs on 64-bit Ubuntu.
# https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/326311
# https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/246911
grab_lib32() {
  echo "Copying 32-bit libs..."
  local SYSLIBS="
    /usr/lib32/libsqlite3.so.0
    /usr/lib32/libsqlite3.so.0.8.6
    /usr/lib32/libnspr4.so.0d
    /usr/lib32/libplds4.so.0d
    /usr/lib32/libplc4.so.0d
    /usr/lib32/libssl3.so.1d
    /usr/lib32/libnss3.so.1d
    /usr/lib32/libsmime3.so.1d
    /usr/lib32/libnssutil3.so.1d
    /usr/lib32/nss/libfreebl3.so
    /usr/lib32/nss/libsoftokn3.chk
    /usr/lib32/nss/libsoftokn3.so
    /usr/lib32/nss/libnssckbi.so
    /usr/lib32/nss/libnssdbm3.so
    /usr/lib32/nss/libfreebl3.chk
    "
  for lib in $SYSLIBS; do
    if [ -h "$lib" ]; then
      cp -P "$lib" "${STAGEDIR}${INSTALLDIR}/"
    else
      install "$lib" "${STAGEDIR}${INSTALLDIR}/"
    fi
  done
}

# Prepare and create the 32-bit package.
package_32() {
  export DEB_HOST_ARCH=i386
  echo "Packaging ${DEB_HOST_ARCH}..."
  DEB_DEPS="libc6 (>= 2.6-1), libnss3-1d, libatk1.0-0 (>= 1.13.2), \
    libcairo2 (>=1.4.0), libfontconfig1 (>= 2.4.0), libfreetype6 (>= 2.3.5), \
    libgcc1 (>= 1:4.2.1), libglib2.0-0 (>= 2.14.0), libgtk2.0-0 (>= 2.12.0), \
    libnspr4-0d (>= 1.8.0.10), libpango1.0-0 (>= 1.18.3), \
    libstdc++6 (>= 4.2.1), zlib1g (>= 1:1.2.3.3.dfsg-1), ${COMMON_DEPS}"
  gen_changelog
  process_template control.template control
  do_package
}

# Prepare and create the 64-bit package.
package_64() {
  export DEB_HOST_ARCH=amd64
  echo "Packaging ${DEB_HOST_ARCH}..."
  DEB_DEPS="ia32-libs (>= 1.6), lib32gcc1 (>= 1:4.1.1), \
    lib32stdc++6 (>= 4.1.1), lib32z1 (>= 1:1.1.4), libc6-i386 (>= 2.4), \
    ${COMMON_DEPS}"
  gen_changelog
  grab_lib32
  process_template control.template control
  do_package
}

# Prepare and create the 64-bit package without 32-bit compat libs.
package_split_64() {
  export DEB_HOST_ARCH=amd64
  # ia32-libs should eventually provide all the missing 32-bit libs, so when it
  # does, we can rewrite the compat-lib32 dep as:
  #   ia32-libs (>= 2.7ubuntuXYZ) | chromium-compat-lib32
  # This will allow either ia32-libs or our package to provide the 32-bit libs,
  # depending on the platform.
  # Note that this won't replace the current ia32-libs dep because we still need
  # at least that version for other dependencies, like libgtk.
  DEB_DEPS="chromium-compat-lib32, \
    ia32-libs (>= 1.6), lib32gcc1 (>= 1:4.1.1), lib32stdc++6 (>= 4.1.1), \
    lib32z1 (>= 1:1.1.4), libc6-i386 (>= 2.4), ${COMMON_DEPS}"
  DEB_REPLACES="${PACKAGE}"
  DEB_CONFLICTS="${PACKAGE}"
  process_template control.template control
  do_package
}

# Prepare and create the package of 32-bit library dependencies.
package_compat_libs() {
  cleanup
  export DEB_HOST_ARCH=amd64
  PACKAGE="chromium-compat-lib32"
  echo "Packaging ${PACKAGE}..."
  SHORTDESC="32-bit compatibility libs for Chromium browser."
  FULLDESC="32-bit libs needed by the Chromium web browser, which are not \
  provided by ia32-libs on Hardy."
  DEB_DEPS=""
  DEB_PROVIDES=""
  DEB_REPLACES="chromium-compat-lib32"
  DEB_CONFLICTS="chromium-compat-lib32"
  gen_changelog
  install -m 755 -d "${STAGEDIR}/DEBIAN" "${STAGEDIR}${INSTALLDIR}"
  grab_lib32
  process_template control.template control
  do_package
}


# Actually generate the package file.
do_package() {
  if [ -f control ]; then
    gen_control
  fi
  fakeroot dpkg-deb -b "${STAGEDIR}" .
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  echo "Cleaning..."
  rm -rf "${STAGEDIR}"
  rm -f "${CHANGELOG}"
  rm -f files
  rm -f control
}

#=========
# MAIN
#=========

BUILDDIR=../../../Hammer
STAGEDIR=build
CHANGELOG=changelog.auto
cleanup

if [ "$1" = "release" ]; then
  source ../common/google-chrome/google-chrome.info
else
  source ../common/chromium-browser/chromium-browser.info
fi
# Some Debian packaging tools want these set.
export DEBFULLNAME="${MAINTNAME}"
export DEBEMAIL="${MAINTMAIL}"
get_versioninfo

prep_staging
stage_install
COMMON_DEPS="msttcorefonts, xdg-utils (>= 1.0.1)"
package_32
package_64

# TODO(mmoss) At some point we might want to separate the 32-bit compatibility
# libs into a separate package, to decrease the download size for distros that
# don't need the additional libs, and to make sure we're using the updated
# system libs if available (i.e. once Ubuntu bug
# https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/326311 is fixed).
#
# The problem is we also want to provide a "one-click" install, which means the
# first installed package (as created above) has to have everything it needs,
# since we can't assume people have our repository configured to download
# additional packages from us to satisfy dependencies. Unfortunately, creating
# such a "monolithic" package for the first install breaks the ability to rely
# on the system's package manager for autoupdates. This is because we would be
# trying to update the monolithic package with the split packages, but the split
# browser package adds a dependency on the split library package, and 'apt-get
# upgrade' refuses to upgrade a package to a newer version if the dependency
# list has changed. For that you need 'apt-get dist-upgrade', which is not
# appropriate for daily updates, or an explicit 'apt-get install <package>',
# which we might be able to do if we use a custom auto-update script, but that
# is still to be decided for post-dogfood.
#
# For now, the safest bet is to use the monolithic package for everything, maybe
# even until the next Ubuntu LTS release with (hopefully) fixed ia32.
#
# Note that it's not sufficient to add the library dependency and a faux
# "Provides" to the monolithic package, since apt-get seems to be smart enough
# to negate those out, and thus still considers the dependency lists to be
# different.
#package_split_64
#package_compat_libs

cleanup
