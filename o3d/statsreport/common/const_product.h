/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Product-specific constants.
//

#ifndef O3D_STATSREPORT_COMMON_CONST_PRODUCT_H_
#define O3D_STATSREPORT_COMMON_CONST_PRODUCT_H_

#include <build/build_config.h>
#include "base/basictypes.h"

#define _QUOTEME(x) #x
#define QUOTEME(x) _QUOTEME(x)

#define _WIDEN(X) L ## X
#define WIDEN(X) _WIDEN(X)

#define PRODUCT_NAME_STRING "o3d"
#define PRODUCT_VERSION_STRING QUOTEME(O3D_VERSION_NUMBER)

#define PRODUCT_NAME_STRING_WIDE WIDEN(PRODUCT_NAME_STRING)

// keep these two in sync
#define REGISTRY_KEY_NAME_STRING "o3d"
#define REGISTRY_KEY_STRING "Software\\Google\\O3D"

#ifdef OS_WIN
const char kMetricsLockName[] = PRODUCT_NAME_STRING "MetricsAggregationlock";
#endif

const char kProductName[] = PRODUCT_NAME_STRING;

// TODO: Determine if #define below is needed.
// This is a string value to be found under they key
// at REGISTRY_KEY_STRING. This is written by our
// installer
// #define INSTALL_DIR_VALUE_NAME "InstallDir"

const char kUserAgent[] = "O3D-";

const int kStatsUploadIntervalSec = 24 * 60 * 60;  // once per day
const int kStatsAggregationIntervalMSec = 5 * 60 * 1000;  // every 5 minutes

// Taken from Google Update
// TODO: CRITICAL: Link to actual Google Update docs so that we don't have
// to keep this up to date. It will mess with our logs.
const wchar_t* const kRegValueUserId          = L"ui";
const wchar_t* const kRelativeGoopdateRegPath = L"Software\\Google\\Update\\";

const wchar_t* const kClientstateRegistryKey =
    L"Software\\Google\\Update\\ClientState\\"
    L"{70308795-045C-42DA-8F4E-D452381A7459}";
const wchar_t* const kOptInRegistryKey = L"usagestats";

#define INTERNET_MAX_PATH_LENGTH        2048
#define INTERNET_MAX_SCHEME_LENGTH      32
#define INTERNET_MAX_URL_LENGTH         (INTERNET_MAX_SCHEME_LENGTH   \
                                         + sizeof("://")              \
                                         + INTERNET_MAX_PATH_LENGTH)

#endif  // O3D_STATSREPORT_COMMON_CONST_PRODUCT_H_
