// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include "base/file_path.h"

#define FPL FILE_PATH_LITERAL

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
const FilePath::CharType kExtensionFileExtension[] = FPL("crx");

// filenames
const wchar_t kArchivedHistoryFilename[] = L"Archived History";
const FilePath::CharType kCacheDirname[] = FPL("Cache");
const wchar_t kChromePluginDataDirname[] = L"Plugin Data";
const FilePath::CharType kCookieFilename[] = FPL("Cookies");
const FilePath::CharType kHistoryFilename[] = FPL("History");
const wchar_t kLocalStateFilename[] = L"Local State";
const FilePath::CharType kPreferencesFilename[] = FPL("Preferences");
const FilePath::CharType kSafeBrowsingFilename[] = FPL("Safe Browsing");
const wchar_t kThumbnailsFilename[] = L"Thumbnails";
const wchar_t kUserDataDirname[] = L"User Data";
const FilePath::CharType kUserScriptsDirname[] = FPL("User Scripts");
const FilePath::CharType kWebDataFilename[] = FPL("Web Data");
const FilePath::CharType kBookmarksFileName[] = FPL("Bookmarks");
const FilePath::CharType kHistoryBookmarksFileName[] =
    FPL("Bookmarks From History");
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

