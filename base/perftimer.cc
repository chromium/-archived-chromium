// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/perftimer.h"

#include "base/basictypes.h"
#include "base/logging.h"

static FILE* perf_log_file = NULL;

bool InitPerfLog(const char* log_file) {
  if (perf_log_file) {
    // trying to initialize twice
    NOTREACHED();
    return false;
  }

#if defined(OS_WIN)
  return fopen_s(&perf_log_file, log_file, "w") == 0;
#elif defined(OS_POSIX)
  perf_log_file = fopen(log_file, "w");
  return perf_log_file != NULL;
#endif
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
