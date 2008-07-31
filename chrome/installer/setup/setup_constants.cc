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

#include "chrome/installer/setup/setup_constants.h"

namespace installer {
// Elements that makes up install target path.
const wchar_t kChromeOldExe[] = L"old_chrome.exe";
const wchar_t kChromeNewExe[] = L"new_chrome.exe";
const wchar_t kWowHelperExe[] = L"wow_helper.exe";
const wchar_t kDictionaries[] = L"Dictionaries";
const wchar_t kChromeArchive[] = L"chrome.7z";
const wchar_t kChromePatchArchive[] = L"patch.7z";
const wchar_t kChromeCompressedArchive[] = L"chrome.packed.7z";
const wchar_t kChromeCompressedPatchArchivePrefix[] = L"patch";

// Sub directory of install source package under install temporary directory.
const wchar_t kInstallSourceDir[] = L"source";
const wchar_t kInstallSourceChromeDir[] = L"Chrome-bin";

const wchar_t kInstallerDir[] = L"Installer";

const wchar_t kMediaPlayerRegPath[] = L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList";
}  // namespace installer
