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

#include "base/sys_string_conversions.h"

#include "base/string_util.h"

namespace base {

std::string SysWideToUTF8(const std::wstring& wide) {
  // In theory this should be using the system-provided conversion rather
  // than our ICU, but this will do for now.
  return WideToUTF8(wide);
}
std::wstring SysUTF8ToWide(const std::string& utf8) {
  // In theory this should be using the system-provided conversion rather
  // than our ICU, but this will do for now.
  return UTF8ToWide(utf8);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  // TODO(evanm): we can't assume Linux is UTF-8.
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(const std::string& native_mb) {
  // TODO(evanm): we can't assume Linux is UTF-8.
  return SysUTF8ToWide(native_mb);
}

}  // namespace base
