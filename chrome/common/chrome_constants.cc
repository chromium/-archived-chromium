// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

namespace chrome {
// The following should not be used for UI strings; they are meant
// for system strings only. UI changes should be made in the GRD.
const wchar_t kBrowserProcessExecutableName[] = L"chrome.exe";
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kBrowserAppName[] = L"Chrome";
const char    kStatsFilename[] = "ChromeStats2";
#else
const wchar_t kBrowserAppName[] = L"Chromium";
const char    kStatsFilename[] = "ChromiumStats2";
#endif
const wchar_t kExternalTabWindowClass[] = L"Chrome_ExternalTabContainer";
const wchar_t kMessageWindowClass[] = L"Chrome_MessageWindow";
const wchar_t kCrashReportLog[] = L"Reported Crashes.txt";
const wchar_t kTestingInterfaceDLL[] = L"testing_interface.dll";
const wchar_t kNotSignedInProfile[] = L"Default";
const wchar_t kNotSignedInID[] = L"not-signed-in";
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
const wchar_t kUserScriptsDirname[] = L"User Scripts";
const wchar_t kWebDataFilename[] = L"Web Data";
const wchar_t kBookmarksFileName[] = L"Bookmarks";
const wchar_t kHistoryBookmarksFileName[] = L"Bookmarks From History";
const wchar_t kCustomDictionaryFileName[] = L"Custom Dictionary.txt";

// Note, this shouldn't go above 64.  See bug 535234.
const unsigned int kMaxRendererProcessCount = 20;
const int kStatsMaxThreads = 32;
const int kStatsMaxCounters = 300;

// We don't enable record mode in the released product because users could
// potentially be tricked into running a product in record mode without
// knowing it.  Enable in debug builds.  Playback mode is allowed always,
// because it is useful for testing and not hazardous by itself.
#ifndef NDEBUG
const bool kRecordModeEnabled = true;
#else
const bool kRecordModeEnabled = false;
#endif
}

