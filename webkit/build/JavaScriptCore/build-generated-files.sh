#!/usr/bin/bash

# This file just sets some paths then defers file creation to
# DerivedSources.make.
# This file is almost the same as the one found in webkit.  The only
# difference is that some paths have changed and we don't try to detect
# the number of CPUs.

NUMCPUS=1

XSRCROOT="`pwd`/../.."
XSRCROOT=`realpath "$XSRCROOT"`
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XSRCROOT=`cygpath -m -s "$XSRCROOT"`
XSRCROOT=`cygpath -u "$XSRCROOT"`
export XSRCROOT
export SOURCE_ROOT=$XSRCROOT

XDSTROOT="$1"
export XDSTROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XDSTROOT=`cygpath -m -s "$XDSTROOT"`
XDSTROOT=`cygpath -u "$XDSTROOT"`
export XDSTROOT

SDKROOT="$2"
export SDKROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SDKROOT=`cygpath -m -s "$SDKROOT"`
SDKROOT=`cygpath -u "$SDKROOT"`
export SDKROOT

export BUILT_PRODUCTS_DIR="$XDSTROOT"

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources"

export JavaScriptCore="${XSRCROOT}/../third_party/WebKit/JavaScriptCore"
export DFTABLES_EXTENSION=".exe"
make -f "$JavaScriptCore/DerivedSources.make" -j ${NUMCPUS} || exit 1
