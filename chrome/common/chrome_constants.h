// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Chrome application.

#ifndef CHROME_COMMON_CHROME_CONSTANTS_H__
#define CHROME_COMMON_CHROME_CONSTANTS_H__

#include "base/file_path.h"

namespace chrome {

extern const wchar_t kBrowserProcessExecutableName[];
extern const wchar_t kBrowserAppName[];
extern const wchar_t kMessageWindowClass[];
extern const wchar_t kExternalTabWindowClass[];
extern const wchar_t kCrashReportLog[];
extern const wchar_t kTestingInterfaceDLL[];
extern const wchar_t kNotSignedInProfile[];
extern const wchar_t kNotSignedInID[];
extern const char    kStatsFilename[];
extern const wchar_t kBrowserResourcesDll[];
extern const FilePath::CharType kExtensionFileExtension[];

// filenames
extern const wchar_t kArchivedHistoryFilename[];
extern const FilePath::CharType kCacheDirname[];
extern const wchar_t kChromePluginDataDirname[];
extern const FilePath::CharType kCookieFilename[];
extern const FilePath::CharType kHistoryFilename[];
extern const wchar_t kLocalStateFilename[];
extern const FilePath::CharType kPreferencesFilename[];
extern const FilePath::CharType kSafeBrowsingFilename[];
extern const wchar_t kThumbnailsFilename[];
extern const wchar_t kUserDataDirname[];
extern const FilePath::CharType kUserScriptsDirname[];
extern const FilePath::CharType kWebDataFilename[];
extern const wchar_t kBookmarksFileName[];
extern const wchar_t kHistoryBookmarksFileName[];
extern const wchar_t kCustomDictionaryFileName[];

extern const unsigned int kMaxRendererProcessCount;
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

extern const bool kRecordModeEnabled;
}

#endif  // CHROME_COMMON_CHROME_CONSTANTS_H__

