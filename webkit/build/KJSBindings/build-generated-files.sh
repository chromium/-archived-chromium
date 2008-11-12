#!/usr/bin/bash

XSRCROOT="$1"
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XSRCROOT=`cygpath -m -s "$XSRCROOT"`
XSRCROOT=`cygpath -u "$XSRCROOT"`
export XSRCROOT
export SOURCE_ROOT=$XSRCROOT

PORTROOT="$2"
PORTROOT=`cygpath -m -s "$PORTROOT"`
PORTROOT=`cygpath -u "$PORTROOT"`
export PORTROOT=`realpath "$PORTROOT"`

XDSTROOT="$3"
export XDSTROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XDSTROOT=`cygpath -m -s "$XDSTROOT"`
XDSTROOT=`cygpath -u "$XDSTROOT"`
export XDSTROOT

export BUILT_PRODUCTS_DIR="$4"
export CREATE_HASH_TABLE="$XSRCROOT/../JavaScriptCore/create_hash_table"

DerivedSourcesDir="${BUILT_PRODUCTS_DIR}\DerivedSources"
echo "$DerivedSourcesDir"
mkdir -p "$DerivedSourcesDir"
cd "$DerivedSourcesDir"

export WebCore="${XSRCROOT}"
export ENCODINGS_FILE="${WebCore}/platform/win/win-encodings.txt";
export ENCODINGS_PREFIX=""

# To see what FEATURE_DEFINES Apple uses, look at:
# webkit/third_party/WebCore/Configurations/WebCore.xcconfig
export FEATURE_DEFINES="ENABLE_SVG ENABLE_SVG_ANIMATION ENABLE_SVG_AS_IMAGE ENABLE_SVG_FONTS ENABLE_SVG_FOREIGN_OBJECT ENABLE_SVG_USE ENABLE_XPATH ENABLE_XSLT"

make -f "$WebCore/DerivedSources.make" -j 2 || exit 1
