// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_constants.h"

namespace installer {
// Elements that makes up install target path.
const wchar_t kWowHelperExe[] = L"wow_helper.exe";
const wchar_t kDictionaries[] = L"Dictionaries";
const wchar_t kChromeArchive[] = L"chrome.7z";
const wchar_t kChromePatchArchive[] = L"patch.7z";
const wchar_t kChromeCompressedArchive[] = L"chrome.packed.7z";
const wchar_t kChromeCompressedPatchArchivePrefix[] = L"patch";

// Sub directory of install source package under install temporary directory.
const wchar_t kInstallSourceDir[] = L"source";
const wchar_t kInstallSourceChromeDir[] = L"Chrome-bin";

const wchar_t kMediaPlayerRegPath[] = L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList";
}  // namespace installer

