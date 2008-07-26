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

// Defines all the command-line switches used with Google Update.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H__
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H__

namespace google_update {

extern const wchar_t kChromeGuid[];

// Strictly speaking Google Update doesn't care about this GUID but it is still
// related to install as it is used by MSI to identify Gears.
extern const wchar_t kGearsUpgradeCode[];

extern const wchar_t kRegPathClients[];
extern const wchar_t kRegPathClientState[];
extern const wchar_t kRegApFieldName[];
extern const wchar_t kRegNameField[];
extern const wchar_t kRegVersionField[];
extern const wchar_t kRegLastCheckedField[];

}  // namespace google_update

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H__
