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

#ifndef CHROME_COMMON_WIN_SAFE_UTIL_H__
#define CHROME_COMMON_WIN_SAFE_UTIL_H__

#include <string>
#include <windows.h>

namespace win_util {

// Open or run a downloaded file via the Windows shell, possibly showing first
// a consent dialog if the the file is deemed dangerous. This function is an
// enhancement over the OpenItemViaShell() function of win_util.h.
//
// The user consent dialog will be shown or not according to the windows
// execution policy defined in the registry which can be overridden per user.
// The mechanics of the policy are explained in the Microsoft Knowledge base
// number 883260: http://support.microsoft.com/kb/883260
//
// The 'hwnd' is the handle to the parent window. In case a dialog is displayed
// the parent window will be disabled since the dialog is meant to be modal.
// The 'window_title' is the text displayed on the title bar of the dialog. If
// you pass an empty string the dialog will have a generic 'windows security'
// name on the title bar.
//
// You must provide a valid 'full_path' to the file to be opened and a well
// formed url in 'source_url'. The url should identify the source of the file
// but does not have to be network-reachable. If the url is malformed a
// dialog will be shown telling the user that the file will be blocked.
//
// In the event that there is no default application registered for the file
// specified by 'full_path' it ask the user, via the Windows "Open With"
// dialog, for an application to use if 'ask_for_app' is true.
// Returns 'true' on successful open, 'false' otherwise.
bool SaferOpenItemViaShell(HWND hwnd, const std::wstring& window_title,
                           const std::wstring& full_path,
                           const std::wstring& source_url, bool ask_for_app);

// Sets the Zone Identifier on the file to "Internet" (3). Returns true if the
// function succeeds, false otherwise. A failure is expected on system where
// the Zone Identifier is not supported, like a machine with a FAT32 filesystem.
// It should not be considered fatal.
bool SetInternetZoneIdentifier(const std::wstring& full_path);

}  // namespace win_util

#endif // CHROME_COMMON_WIN_SAFE_UTIL_H__
