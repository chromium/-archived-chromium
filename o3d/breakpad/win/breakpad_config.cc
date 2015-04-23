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

//
// Breakpad configuration constants

// Product-specific constants.  MODIFY THESE TO SUIT YOUR PROJECT.

#define _QUOTEME(x) #x
#define QUOTEME(x) _QUOTEME(x)

#define _MAKE_LONG_STRING(string) L ## string
#define MAKE_LONG_STRING(string) _MAKE_LONG_STRING(string)

#define PRODUCT_VERSION_STRING MAKE_LONG_STRING(QUOTEME(O3D_VERSION_NUMBER))

// !!@ CURRENTLY WE'RE HARDCODING (win32 firefox) HERE!!
wchar_t *kCrashReportProductName = L"O3D";  // [naming]

wchar_t *kCrashReportProductVersion =
    PRODUCT_VERSION_STRING L" (win32 firefox)";

// Crash report uploading configuration (used by reporter.exe)

// production server
wchar_t *kCrashReportUrl = L"http://www.google.com/cr/report";

wchar_t *kCrashReportProductParam = L"prod";
wchar_t *kCrashReportVersionParam = L"ver";

// Throttling-related constants
// (we don't want to upload too many crash reports in a row)

bool kCrashReportAlwaysUpload = false;  // disables throttling if set to |true|

wchar_t *kCrashReportThrottlingRegKey =
    L"Software\\Google\\Breakpad\\Throttling";

int kCrashReportAttempts         = 3;
int kCrashReportResendPeriodMs   = (1 * 60 * 60 * 1000);

// kCrashReportsMaxPerInterval is defined (using #define)
// in the header file, since the compiler requires it to be a constant
// (can't be global variable as defined in this file)

int kCrashReportsIntervalSeconds = (24 * 60  * 60);
