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

#include <stdio.h>
#include <shlwapi.h>
#include <windows.h>

#include "base/perftimer.h"

#include "base/logging.h"
#include "base/basictypes.h"

static FILE* perf_log_file = NULL;

bool InitPerfLog(const char* log_file) {
  if (perf_log_file) {
    // trying to initialize twice
    NOTREACHED();
    return false;
  }

  return fopen_s(&perf_log_file, log_file, "w") == 0;
}

void FinalizePerfLog() {
  if (!perf_log_file) {
    // trying to cleanup without initializing
    NOTREACHED();
    return;
  }
  fclose(perf_log_file);
}

void LogPerfResult(const char* test_name, double value, const char* units) {
  if (!perf_log_file) {
    NOTREACHED();
    return;
  }

  fprintf(perf_log_file, "%s\t%g\t%s\n", test_name, value, units);
  printf("%s\t%g\t%s\n", test_name, value, units);
}

