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

#include "base/string_util.h"

#include <pthread.h>
#include <string>
#include <vector>
#include "base/logging.h"
#include "unicode/numfmt.h"

static NumberFormat* number_format_singleton = NULL;

static void DoInitializeStatics() {
  UErrorCode status = U_ZERO_ERROR;
  number_format_singleton = NumberFormat::createInstance(status);
  DCHECK(U_SUCCESS(status));
}

static void InitializeStatics() {
  static pthread_once_t pthread_once_initialized = PTHREAD_ONCE_INIT;
  pthread_once(&pthread_once_initialized, DoInitializeStatics);
}

// Technically, the native multibyte encoding would be the encoding returned
// by CFStringGetSystemEncoding or GetApplicationTextEncoding, but I can't
// imagine anyone needing or using that from these APIs, so just treat UTF-8
// as though it were the native multibyte encoding.
std::string WideToNativeMB(const std::wstring& wide) {
  return WideToUTF8(wide);
}

std::wstring NativeMBToWide(const std::string& native_mb) {
  return UTF8ToWide(native_mb);
}

NumberFormat* NumberFormatSingleton() {
  InitializeStatics();
  return number_format_singleton;
}
