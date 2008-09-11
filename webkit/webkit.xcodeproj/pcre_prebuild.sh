#!/bin/sh

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex
GENERATED_DIR="${CONFIGURATION_TEMP_DIR}/generated"
OUTDIR="${GENERATED_DIR}/pcre"
CHARTABLES="${OUTDIR}/chartables.c"
DFTABLES="${SOURCE_ROOT}/../third_party/WebKit/JavaScriptCore/pcre/dftables"

mkdir -p "${OUTDIR}"
perl -w "${DFTABLES}" "${CHARTABLES}"
