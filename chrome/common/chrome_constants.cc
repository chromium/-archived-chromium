// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/common/chrome_constants.h"

namespace chrome {
// The following should not be used for UI strings; they are meant
// for system strings only. UI changes should be made in the GRD.
const wchar_t kBrowserProcessExecutableName[] = L"chrome.exe";
const wchar_t kBrowserAppName[] = L"Chrome";
const wchar_t kMessageWindowClass[] = L"Chrome_MessageWindow";
const wchar_t kExternalTabWindowClass[] = L"Chrome_ExternalTabContainer";
const wchar_t kCrashReportLog[] = L"Reported Crashes.txt";
const wchar_t kTestingInterfaceDLL[] = L"testing_interface.dll";
const wchar_t kNotSignedInProfile[] = L"Default";
const wchar_t kNotSignedInID[] = L"not-signed-in";
const wchar_t kStatsFilename[] = L"ChromeStats";
const wchar_t kBrowserResourcesDll[] = L"chrome.dll";

// filenames
const wchar_t kArchivedHistoryFilename[] = L"Archived History";
const wchar_t kCacheDirname[] = L"Cache";
const wchar_t kChromePluginDataDirname[] = L"Plugin Data";
const wchar_t kCookieFilename[] = L"Cookies";
const wchar_t kHistoryFilename[] = L"History";
const wchar_t kLocalStateFilename[] = L"Local State";
const wchar_t kPreferencesFilename[] = L"Preferences";
const wchar_t kSafeBrowsingFilename[] = L"Safe Browsing";
const wchar_t kThumbnailsFilename[] = L"Thumbnails";
const wchar_t kUserDataDirname[] = L"User Data";
const wchar_t kWebDataFilename[] = L"Web Data";

// Note, this shouldn't go above 64.  See bug 535234.
const unsigned int kMaxRendererProcessCount = 20;
const int kStatsMaxThreads = 32;
const int kStatsMaxCounters = 300;

// We don't enable record mode in the released product because users could
// potentially be tricked into running a product in record mode without
// knowing it.  Enable in debug builds.  Playback mode is allowed always,
// because it is useful for testing and not hazardous by itself.
#ifdef DEBUG
const bool kRecordModeEnabled = true;
#else
const bool kRecordModeEnabled = false;
#endif
}
