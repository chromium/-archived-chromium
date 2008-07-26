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

#include "base/base_switches.h"

namespace switches {

// If the program includes chrome/common/debug_on_start.h, the process will
// start the JIT system-registered debugger on itself and will wait for 60
// seconds for the debugger to attach to itself. Then a break point will be hit.
const wchar_t kDebugOnStart[]                  = L"debug-on-start";

// Will wait for 60 seconds for a debugger to come to attach to the process.
const wchar_t kWaitForDebugger[]               = L"wait-for-debugger";

// Suppresses all error dialogs when present.
const wchar_t kNoErrorDialogs[]                = L"noerrdialogs";

// Disables the crash reporting.
const wchar_t kDisableBreakpad[]               = L"disable-breakpad";

// Generates full memory crash dump.
const wchar_t kFullMemoryCrashReport[]         = L"full-memory-crash-report";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const wchar_t kProcessType[]                   = L"type";

// Enable DCHECKs in release mode.
const wchar_t kEnableDCHECK[]                  = L"enable-dcheck";

}  // namespace switches
